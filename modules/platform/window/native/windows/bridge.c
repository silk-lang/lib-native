// The Win32 backend of modules/platform/window: native/bridge.h over USER32, IMM32 and DWM.
//
// Like the macOS backend, the system's modal loops (live resize and move, menus, the file
// dialog) would stop a pull-model event loop until they end. Here the message pump runs on
// a second fiber of the platform's thread: silk_window_next_event switches to it, and the
// window procedure switches back as soon as an event is queued, from wherever Windows is.
// During a live resize a timer keeps the pump fiber waking, so frames keep coming. Set
// SILK_WINDOW_FIBER=0 to pump on the caller's stack instead.
//
// Frames are paced by DwmFlush on a helper thread, which signals an event at each
// composition while any window waits for a frame.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <imm.h>
#include <shellscalingapi.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/core.h"
#include "../common/cursors.h"
#include "keys.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

enum {
    WM_SILK_RUN = WM_APP + 1,       // run a deferred task on the pump fiber
    WM_SILK_KEYSTATE = WM_APP + 2,  // test hook: restore the keyboard state
    WM_SILK_WAKE = WM_APP + 3,      // silk_window_wake: return to Silk
    WM_SILK_TRAY = WM_APP + 4,      // the notification area's callback for our icons
    TRAY_ICON = 1,                  // the tray icon's id
    NOTICE_ICON = 2,                // an icon added only to show a notification
    TIMER_MODAL = 1,
    MENU_ACTION_BASE = 0x1000,
    MENU_SYSTEM_BASE = 0x100,
};

typedef struct Win32Window {
    uint32_t id;
    HWND hwnd;
    HMENU menu;
    bool open;
    bool visible;
    bool minimized;
    bool focused;
    bool resizable;
    bool titleHidden;
    bool fullScreen;
    bool tracking; // TrackMouseEvent armed
    WINDOWPLACEMENT placement;
    LONG_PTR savedStyle;
    int pixelWidth, pixelHeight;
    double scale;
    double minWidth, minHeight;
    struct { bool pending, forced; } frame;
    uint32_t cursor;
    HCURSOR custom; // from set_cursor_image, while it shows
    bool imeEnabled;
    HIMC savedContext;
    double imeX, imeY, imeWidth, imeHeight;
    WCHAR highSurrogate;
    double lastClickTime;
    int lastClickX, lastClickY;
    uint32_t lastClickButton, clickCount;
    IDropTarget *dropTarget;
} Win32Window;

typedef struct Task {
    int kind; // 1 open dialog, 2 save dialog, 3 window drag, 4 context menu, 5 file drag
    uint32_t id;
    POINT point;
    HMENU menu;
    uint32_t window;
    uint32_t flags;
    WCHAR *title, *directory, *name;
    char **extensions;
} Task;

static bool started;
static HINSTANCE instance;
static ATOM windowClass;
static HWND helper;
static Win32Window *windows[256];
static size_t windowCount;
static uint32_t nextWindow = 1;
static double refreshInterval = 1.0 / 60.0;
static uint32_t appearanceValue;

// Fibers.
static bool fiberMode;
static void *mainFiber;
static void *pumpFiber;
static bool onPump;
static double deadline;

// Frame pacing.
static HANDLE vblankEvent;
static HANDLE wantVblank;


static Win32Window *find(uint32_t id) {
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->id == id && windows[i]->open) return windows[i];
    return NULL;
}

static Win32Window *find_hwnd(HWND hwnd) {
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->hwnd == hwnd && windows[i]->open) return windows[i];
    return NULL;
}

// ---------------------------------------------------------------------------------------
// Strings.

static WCHAR *wide(const uint8_t *text, size_t length) {
    int count = length ? MultiByteToWideChar(CP_UTF8, 0, (const char *)text, (int)length, NULL, 0) : 0;
    WCHAR *out = malloc(((size_t)count + 1) * sizeof(WCHAR));
    if (!out) abort();
    if (count) MultiByteToWideChar(CP_UTF8, 0, (const char *)text, (int)length, out, count);
    out[count] = 0;
    return out;
}

static uint8_t *narrow(const WCHAR *text, int count, size_t *length) {
    if (count < 0) count = (int)wcslen(text);
    int bytes = count ? WideCharToMultiByte(CP_UTF8, 0, text, count, NULL, 0, NULL, NULL) : 0;
    uint8_t *out = malloc((size_t)bytes + 1);
    if (!out) abort();
    if (bytes) WideCharToMultiByte(CP_UTF8, 0, text, count, (char *)out, bytes, NULL, NULL);
    out[bytes] = 0;
    *length = (size_t)bytes;
    return out;
}

// ---------------------------------------------------------------------------------------
// DPI.

static UINT dpi_of(HWND hwnd) {
    static UINT(WINAPI * getDpiForWindow)(HWND);
    static bool looked;
    if (!looked) {
        looked = true;
        getDpiForWindow = (UINT(WINAPI *)(HWND))(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    }
    if (hwnd && getDpiForWindow) return getDpiForWindow(hwnd);
    HDC dc = GetDC(NULL);
    UINT dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    return dpi ? dpi : 96;
}

static void adjust_rect(RECT *rect, DWORD style, DWORD exStyle, bool menu, UINT dpi) {
    static BOOL(WINAPI * adjustForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT);
    static bool looked;
    if (!looked) {
        looked = true;
        adjustForDpi = (BOOL(WINAPI *)(LPRECT, DWORD, BOOL, DWORD, UINT))(void *)GetProcAddress(
            GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi");
    }
    if (adjustForDpi) adjustForDpi(rect, style, menu, exStyle, dpi);
    else AdjustWindowRectEx(rect, style, menu, exStyle);
}

static void enable_dpi_awareness(void) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    typedef BOOL(WINAPI * SetContext)(HANDLE);
    SetContext setContext = (SetContext)(void *)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (setContext && setContext((HANDLE)-4)) return; // PER_MONITOR_AWARE_V2
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI * SetAwareness)(int);
        SetAwareness setAwareness = (SetAwareness)(void *)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setAwareness && SUCCEEDED(setAwareness(2))) return;
    }
    SetProcessDPIAware();
}

// ---------------------------------------------------------------------------------------
// Appearance.

static uint32_t read_appearance(void) {
    DWORD value = 1, size = sizeof value;
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size) != ERROR_SUCCESS)
        return 0;
    return value == 0 ? 1 : 0;
}

static void apply_dark_title(HWND hwnd) {
    BOOL dark = appearanceValue == 1;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof dark);
}

// ---------------------------------------------------------------------------------------
// The pump fiber and frame pacing.

static DWORD WINAPI vblank_thread(LPVOID unused) {
    (void)unused;
    for (;;) {
        WaitForSingleObject(wantVblank, INFINITE);
        if (FAILED(DwmFlush())) Sleep((DWORD)(refreshInterval * 1000.0));
        SetEvent(vblankEvent);
    }
    return 0;
}

static bool any_frame_waiting(void) {
    for (size_t i = 0; i < windowCount; i++) {
        Win32Window *window = windows[i];
        if (window->open && window->frame.pending && !window->frame.forced && window->visible &&
            !window->minimized)
            return true;
    }
    return false;
}

static void deliver_frames(bool vblank) {
    double now = silk_now();
    for (size_t i = 0; i < windowCount; i++) {
        Win32Window *window = windows[i];
        if (!window->open || !window->frame.pending || !window->visible || window->minimized) continue;
        if (!window->frame.forced && !vblank) continue;
        window->frame.pending = window->frame.forced = false;
        SilkWindowEvent event = silk_event(SILK_EVENT_REQUEST_FRAME, window->id);
        event.value = now + refreshInterval;
        silk_queue_push(&event);
    }
    if (any_frame_waiting()) SetEvent(wantVblank);
}

static void check_vblank(void) {
    if (WaitForSingleObject(vblankEvent, 0) == WAIT_OBJECT_0) deliver_frames(true);
}

// From the pump fiber: hands control back to Silk when there is something to return or
// the wait is over. Safe to call from inside the system's modal loops.
// Set by silk_window_wake from any thread, cleared when silk_window_next_event returns.
static volatile LONG woken;

static bool wake_pending(void) { return InterlockedCompareExchange(&woken, 0, 0) != 0; }

static void yield_if_ready(void) {
    if (!fiberMode || !onPump) return;
    deliver_frames(false);
    if (!silk_queue_empty() || silk_now() >= deadline || wake_pending()) {
        onPump = false;
        SwitchToFiber(mainFiber);
        onPump = true;
    }
}

static bool pre_translate(MSG *message);

static void pump_once(DWORD wait) {
    // The wait resets the auto-reset event when it is what woke us, so act on the result.
    DWORD woke = MsgWaitForMultipleObjectsEx(1, &vblankEvent, wait, QS_ALLINPUT,
                                             MWMO_INPUTAVAILABLE);
    if (woke == WAIT_OBJECT_0) deliver_frames(true);
    else check_vblank();
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) continue;
        if (pre_translate(&message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
        check_vblank();
    }
    deliver_frames(false);
}

static DWORD wait_millis(void) {
    double now = silk_now();
    if (deadline == INFINITY) return INFINITE;
    double left = (deadline - now) * 1000.0;
    return left <= 0 ? 0 : (DWORD)ceil(left);
}

static void CALLBACK pump_fiber(void *unused) {
    (void)unused;
    for (;;) {
        onPump = true;
        while (silk_queue_empty() && silk_now() < deadline && !wake_pending())
            pump_once(wait_millis());
        onPump = false;
        SwitchToFiber(mainFiber);
    }
}

// ---------------------------------------------------------------------------------------
// Start.

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

int32_t silk_window_start(void) {
    if (started) return 1;
    instance = GetModuleHandleW(NULL);
    enable_dpi_awareness();
    HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    (void)com; // RPC_E_CHANGED_MODE: the app already chose; dialogs still work in MTA.
    // Dropping files needs OLE, which needs the single-threaded apartment.
    HRESULT ole = OleInitialize(NULL);
    (void)ole;
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    wc.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"SilkWindow";
    windowClass = RegisterClassExW(&wc);
    if (!windowClass) return 3;
    helper = CreateWindowExW(0, L"SilkWindow", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, instance, NULL);
    vblankEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    wantVblank = CreateEventW(NULL, FALSE, FALSE, NULL);
    HANDLE thread = CreateThread(NULL, 0, vblank_thread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
    DEVMODEW mode;
    memset(&mode, 0, sizeof mode);
    mode.dmSize = sizeof mode;
    if (EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1)
        refreshInterval = 1.0 / mode.dmDisplayFrequency;
    appearanceValue = read_appearance();
    const char *fiber = getenv("SILK_WINDOW_FIBER");
    fiberMode = !(fiber && strcmp(fiber, "0") == 0);
    if (fiberMode) {
        mainFiber = ConvertThreadToFiber(NULL);
        if (!mainFiber && GetLastError() == ERROR_ALREADY_FIBER) mainFiber = GetCurrentFiber();
        pumpFiber = mainFiber ? CreateFiber(1024 * 1024, pump_fiber, NULL) : NULL;
        if (!pumpFiber) fiberMode = false;
    }
    started = true;
    return 0;
}

uint32_t silk_window_backend(void) { return started ? SILK_BACKEND_WIN32 : SILK_BACKEND_NONE; }

uint32_t silk_window_appearance(void) { return appearanceValue; }

static uint8_t next_event(SilkWindowEvent *event) {
    for (;;) {
        if (silk_queue_pop(event)) return 1;
        if (wake_pending()) return 0;
        if (fiberMode) {
            SwitchToFiber(pumpFiber);
            if (silk_queue_pop(event)) return 1;
            if (silk_now() >= deadline || wake_pending()) return 0;
        } else {
            pump_once(0);
            if (silk_queue_pop(event)) return 1;
            if (silk_now() >= deadline || wake_pending()) return 0;
            pump_once(wait_millis());
        }
    }
}

uint8_t silk_window_next_event(double timeout, SilkWindowEvent *event) {
    if (!started) return 0;
    deadline = timeout < 0 ? INFINITY : silk_now() + timeout;
    uint8_t got = next_event(event);
    InterlockedExchange(&woken, 0);
    return got;
}

void silk_window_wake(void) {
    if (!started) return;
    if (InterlockedExchange(&woken, 1) == 0) PostMessageW(helper, WM_SILK_WAKE, 0, 0);
}

// ---------------------------------------------------------------------------------------
// Menus.

static void append_shortcut(WCHAR *text, size_t capacity, const SilkMenuEntry *entry) {
    if (!entry->keyKind) return;
    wcsncat(text, L"\t", capacity - wcslen(text) - 1);
    if (entry->modifiers & SILK_MOD_CONTROL) wcsncat(text, L"Ctrl+", capacity - wcslen(text) - 1);
    if (entry->modifiers & SILK_MOD_ALT) wcsncat(text, L"Alt+", capacity - wcslen(text) - 1);
    if (entry->modifiers & SILK_MOD_SHIFT) wcsncat(text, L"Shift+", capacity - wcslen(text) - 1);
    if (entry->modifiers & SILK_MOD_META) wcsncat(text, L"Win+", capacity - wcslen(text) - 1);
    WCHAR key[16] = {0};
    if (entry->keyKind == 1) {
        uint32_t c = entry->keyValue;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 0x10000) key[0] = (WCHAR)c;
    } else if (entry->keyValue >= SILK_NAMED_F1 && entry->keyValue <= SILK_NAMED_F24) {
        wsprintfW(key, L"F%u", entry->keyValue - SILK_NAMED_F1 + 1);
    } else {
        static const struct {
            uint32_t named;
            const WCHAR *name;
        } names[] = {{SILK_NAMED_Enter, L"Enter"}, {SILK_NAMED_Tab, L"Tab"},
                     {SILK_NAMED_Backspace, L"Backspace"}, {SILK_NAMED_Delete, L"Del"},
                     {SILK_NAMED_Escape, L"Esc"}, {SILK_NAMED_ArrowLeft, L"Left"},
                     {SILK_NAMED_ArrowRight, L"Right"}, {SILK_NAMED_ArrowUp, L"Up"},
                     {SILK_NAMED_ArrowDown, L"Down"}, {SILK_NAMED_Home, L"Home"},
                     {SILK_NAMED_End, L"End"}, {SILK_NAMED_PageUp, L"PgUp"},
                     {SILK_NAMED_PageDown, L"PgDn"}};
        for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
            if (names[i].named == entry->keyValue) wcsncpy(key, names[i].name, 15);
    }
    wcsncat(text, key, capacity - wcslen(text) - 1);
}

// A menu bar, or with `popup` a context menu whose items sit at its top level.
static HMENU build_menu_from(const SilkMenuEntry *entries, size_t count, bool popup) {
    if (!count) return NULL;
    HMENU bar = popup ? CreatePopupMenu() : CreateMenu();
    HMENU stack[32];
    int depth = 0;
    stack[0] = bar;
    for (size_t i = 0; i < count; i++) {
        const SilkMenuEntry *entry = &entries[i];
        HMENU current = stack[depth];
        WCHAR text[512] = {0};
        if (entry->title && entry->titleLength) {
            WCHAR *title = wide(entry->title, entry->titleLength);
            wcsncpy(text, title, 300);
            free(title);
        }
        switch (entry->type) {
        case SILK_MENU_BEGIN: {
            HMENU popup = CreatePopupMenu();
            AppendMenuW(current, MF_POPUP | MF_STRING, (UINT_PTR)popup, text);
            if (depth < 31) stack[++depth] = popup;
            break;
        }
        case SILK_MENU_END:
            if (depth > 0) depth--;
            break;
        case SILK_MENU_ACTION: {
            append_shortcut(text, 512, entry);
            UINT flags = MF_STRING;
            if (!entry->enabled) flags |= MF_GRAYED;
            if (entry->checked) flags |= MF_CHECKED;
            AppendMenuW(current, flags, MENU_ACTION_BASE + entry->action, text);
            break;
        }
        case SILK_MENU_SEPARATOR:
            AppendMenuW(current, MF_SEPARATOR, 0, NULL);
            break;
        case SILK_MENU_SYSTEM: {
            const WCHAR *label = NULL;
            switch (entry->item) {
            case SILK_ITEM_QUIT: label = L"E&xit"; break;
            case SILK_ITEM_CLOSE_WINDOW: label = L"&Close"; break;
            case SILK_ITEM_MINIMIZE: label = L"Mi&nimize"; break;
            case SILK_ITEM_ZOOM: label = L"&Maximize"; break;
            case SILK_ITEM_TOGGLE_FULL_SCREEN: label = L"&Full Screen"; break;
            case SILK_ITEM_UNDO: label = L"&Undo\tCtrl+Z"; break;
            case SILK_ITEM_REDO: label = L"&Redo\tCtrl+Y"; break;
            case SILK_ITEM_CUT: label = L"Cu&t\tCtrl+X"; break;
            case SILK_ITEM_COPY: label = L"&Copy\tCtrl+C"; break;
            case SILK_ITEM_PASTE: label = L"&Paste\tCtrl+V"; break;
            case SILK_ITEM_SELECT_ALL: label = L"Select &All\tCtrl+A"; break;
            default: break; // macOS-only items have no counterpart
            }
            if (!label) break;
            wcsncpy(text, label, 300);
            append_shortcut(text, 512, entry);
            AppendMenuW(current, MF_STRING, MENU_SYSTEM_BASE + entry->item, text);
            break;
        }
        default:
            break;
        }
    }
    return bar;
}

static HMENU build_menu(void) { return build_menu_from(silk_menu_entries, silk_menu_count, false); }

static void client_rect_for(Win32Window *window, int width, int height, RECT *rect) {
    rect->left = 0;
    rect->top = 0;
    rect->right = width;
    rect->bottom = height;
    if (window->titleHidden) return;
    DWORD style = (DWORD)GetWindowLongPtrW(window->hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE);
    adjust_rect(rect, style, exStyle, window->menu != NULL, dpi_of(window->hwnd));
}

static void install_menu(Win32Window *window) {
    HMENU old = window->menu;
    window->menu = window->titleHidden ? NULL : build_menu();
    if (!old && !window->menu) return;
    int width = window->pixelWidth, height = window->pixelHeight;
    SetMenu(window->hwnd, window->menu);
    if (old) DestroyMenu(old);
    // Keep the content size: the menu bar takes its space from the frame.
    RECT rect;
    client_rect_for(window, width, height, &rect);
    SetWindowPos(window->hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void silk_window_menu_commit(void) {
    silk_menu_commit_pending();
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->open) install_menu(windows[i]);
}

static void system_item(Win32Window *window, uint32_t item);

static bool shortcut(Win32Window *window, uint32_t character, uint32_t named, uint32_t modifiers) {
    SilkShortcutMatch match = silk_menu_match(character, named, modifiers);
    if (match.type == SILK_MENU_ACTION) {
        SilkWindowEvent event = silk_event(SILK_EVENT_MENU_ACTION, 0);
        event.code = match.action;
        silk_queue_push(&event);
        return true;
    }
    if (match.type == SILK_MENU_SYSTEM) {
        system_item(window, match.action);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------
// Keys.

static uint32_t current_modifiers(void) {
    uint32_t bits = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) bits |= SILK_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) bits |= SILK_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) bits |= SILK_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) bits |= SILK_MOD_META;
    if (GetKeyState(VK_CAPITAL) & 1) bits |= SILK_MOD_CAPS_LOCK;
    return bits;
}

static uint32_t named_of_vk(UINT vk) {
    if (vk >= VK_F1 && vk <= VK_F24) return SILK_NAMED_F1 + (vk - VK_F1);
    switch (vk) {
    case VK_RETURN: return SILK_NAMED_Enter;
    case VK_TAB: return SILK_NAMED_Tab;
    case VK_BACK: return SILK_NAMED_Backspace;
    case VK_DELETE: return SILK_NAMED_Delete;
    case VK_INSERT: return SILK_NAMED_Insert;
    case VK_ESCAPE: return SILK_NAMED_Escape;
    case VK_LEFT: return SILK_NAMED_ArrowLeft;
    case VK_RIGHT: return SILK_NAMED_ArrowRight;
    case VK_UP: return SILK_NAMED_ArrowUp;
    case VK_DOWN: return SILK_NAMED_ArrowDown;
    case VK_HOME: return SILK_NAMED_Home;
    case VK_END: return SILK_NAMED_End;
    case VK_PRIOR: return SILK_NAMED_PageUp;
    case VK_NEXT: return SILK_NAMED_PageDown;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT: return SILK_NAMED_Shift;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL: return SILK_NAMED_Control;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU: return SILK_NAMED_Alt;
    case VK_LWIN:
    case VK_RWIN: return SILK_NAMED_Meta;
    case VK_CAPITAL: return SILK_NAMED_CapsLock;
    case VK_APPS: return SILK_NAMED_ContextMenu;
    case VK_HELP: return SILK_NAMED_Help;
    case VK_NUMLOCK: return SILK_NAMED_NumLock;
    case VK_SCROLL: return SILK_NAMED_ScrollLock;
    case VK_PAUSE: return SILK_NAMED_Pause;
    case VK_SNAPSHOT: return SILK_NAMED_PrintScreen;
    case VK_VOLUME_UP: return SILK_NAMED_AudioVolumeUp;
    case VK_VOLUME_DOWN: return SILK_NAMED_AudioVolumeDown;
    case VK_VOLUME_MUTE: return SILK_NAMED_AudioVolumeMute;
    case VK_KANA: return SILK_NAMED_KanaMode;
    default: return 0;
    }
}

static uint32_t code_of(LPARAM lParam) {
    UINT scan = (UINT)((lParam >> 16) & 0xFF);
    bool extended = (lParam >> 24) & 1;
    return silk_windows_key_codes[(extended ? 256 : 0) + scan];
}

static uint32_t modifier_bit(uint32_t named) {
    switch (named) {
    case SILK_NAMED_Shift: return SILK_MOD_SHIFT;
    case SILK_NAMED_Control: return SILK_MOD_CONTROL;
    case SILK_NAMED_Alt: return SILK_MOD_ALT;
    case SILK_NAMED_Meta: return SILK_MOD_META;
    case SILK_NAMED_CapsLock: return SILK_MOD_CAPS_LOCK;
    default: return 0;
    }
}

// The text a key types with Shift (and Caps Lock) applied and no other modifiers.
static size_t key_text(UINT vk, UINT scan, bool shift, uint8_t out[16], uint32_t *baseOut) {
    BYTE state[256] = {0};
    WCHAR buffer[8];
    if (baseOut) {
        int count = ToUnicode(vk, scan, state, buffer, 8, 4);
        *baseOut = count == 1 ? buffer[0] : 0;
    }
    if (shift) state[VK_SHIFT] = 0x80;
    if (GetKeyState(VK_CAPITAL) & 1) state[VK_CAPITAL] = 1;
    int count = ToUnicode(vk, scan, state, buffer, 8, 4); // 4: keep the kernel's dead-key state
    if (count <= 0) return 0;
    size_t length = 0;
    uint8_t *text = narrow(buffer, count, &length);
    if (length > 15 || silk_utf8_is_control(text, length)) length = 0;
    memcpy(out, text, length);
    free(text);
    return length;
}

static void push_modifiers(Win32Window *window, uint32_t modifiers) {
    SilkWindowEvent event = silk_event(SILK_EVENT_MODIFIERS_CHANGED, window->id);
    event.modifiers = modifiers;
    silk_queue_push(&event);
}

// Runs before TranslateMessage, so that a menu shortcut types no WM_CHAR.
static bool pre_translate(MSG *message) {
    if (message->message != WM_KEYDOWN && message->message != WM_SYSKEYDOWN) return false;
    Win32Window *window = find_hwnd(message->hwnd);
    if (!window) return false;
    uint32_t modifiers = current_modifiers();
    // AltGr arrives as Control+Alt; it types characters rather than shortcuts.
    bool altGr = (modifiers & SILK_MOD_CONTROL) && (modifiers & SILK_MOD_ALT) &&
                 (GetKeyState(VK_RMENU) & 0x8000);
    if (altGr || !(modifiers & (SILK_MOD_CONTROL | SILK_MOD_ALT | SILK_MOD_META))) return false;
    UINT vk = (UINT)message->wParam;
    UINT scan = (UINT)((message->lParam >> 16) & 0xFF);
    uint8_t text[16];
    uint32_t base = 0;
    key_text(vk, scan, false, text, &base);
    uint32_t named = named_of_vk(vk);
    if (!shortcut(window, base, named, modifiers)) return false;
    return true;
}

static void handle_key(Win32Window *window, UINT message, WPARAM wParam, LPARAM lParam) {
    bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    UINT vk = (UINT)wParam;
    bool imeHandled = false;
    if (vk == VK_PROCESSKEY) {
        imeHandled = true;
        vk = ImmGetVirtualKey(window->hwnd);
    }
    UINT scan = (UINT)((lParam >> 16) & 0xFF);
    uint32_t code = code_of(lParam);
    uint32_t named = named_of_vk(vk);
    if (!named && code < SILK_CODE_COUNT) named = silk_code_named_keys[code];
    uint32_t modifiers = current_modifiers();
    uint8_t text[16];
    size_t length = named ? 0 : key_text(vk, scan, (modifiers & SILK_MOD_SHIFT) != 0, text, NULL);
    uint32_t bit = modifier_bit(named);
    SilkWindowEvent event = silk_event(down ? SILK_EVENT_KEY_DOWN : SILK_EVENT_KEY_UP, window->id);
    event.code = code;
    event.named = named;
    event.modifiers = down && bit && bit != SILK_MOD_CAPS_LOCK ? modifiers & ~bit : modifiers;
    if (down && (lParam & (1 << 30))) event.flags |= SILK_FLAG_REPEAT;
    if (imeHandled) event.flags |= SILK_FLAG_IME_HANDLED;
    silk_queue_push_text(&event, text, length);
    if (bit) push_modifiers(window, modifiers);
}

static void handle_char(Win32Window *window, WPARAM wParam) {
    WCHAR unit = (WCHAR)wParam;
    if (unit >= 0xD800 && unit < 0xDC00) {
        window->highSurrogate = unit;
        return;
    }
    WCHAR pair[2];
    int count = 0;
    if (unit >= 0xDC00 && unit < 0xE000) {
        if (!window->highSurrogate) return;
        pair[count++] = window->highSurrogate;
    }
    window->highSurrogate = 0;
    pair[count++] = unit;
    // Control+key types control characters; those are shortcuts, not text.
    if ((current_modifiers() & (SILK_MOD_CONTROL | SILK_MOD_META)) &&
        !((GetKeyState(VK_RMENU) & 0x8000)))
        return;
    size_t length = 0;
    uint8_t *text = narrow(pair, count, &length);
    if (length && !silk_utf8_is_control(text, length)) {
        SilkWindowEvent event = silk_event(SILK_EVENT_TEXT_INPUT, window->id);
        silk_queue_push_text(&event, text, length);
    }
    free(text);
}

// ---------------------------------------------------------------------------------------
// Input methods.

static void push_preedit(Win32Window *window, const WCHAR *text, int count, int caret) {
    size_t length = 0;
    uint8_t *bytes = narrow(text, count, &length);
    size_t caretBytes = 0;
    if (caret >= 0 && caret <= count) {
        uint8_t *prefix = narrow(text, caret, &caretBytes);
        free(prefix);
    } else {
        caretBytes = length;
    }
    SilkWindowEvent event = silk_event(SILK_EVENT_IME_PREEDIT, window->id);
    event.start = event.end = caretBytes;
    silk_queue_push_text(&event, bytes, length);
    free(bytes);
}

static void push_commit(Win32Window *window, const WCHAR *text, int count) {
    size_t length = 0;
    uint8_t *bytes = narrow(text, count, &length);
    if (length && !silk_utf8_is_control(bytes, length)) {
        SilkWindowEvent event = silk_event(SILK_EVENT_TEXT_INPUT, window->id);
        silk_queue_push_text(&event, bytes, length);
    }
    free(bytes);
}

static void place_ime(Win32Window *window) {
    HIMC context = ImmGetContext(window->hwnd);
    if (!context) return;
    double s = window->scale;
    COMPOSITIONFORM composition;
    composition.dwStyle = CFS_POINT;
    composition.ptCurrentPos.x = (LONG)lround(window->imeX * s);
    composition.ptCurrentPos.y = (LONG)lround(window->imeY * s);
    ImmSetCompositionWindow(context, &composition);
    CANDIDATEFORM candidate;
    candidate.dwIndex = 0;
    candidate.dwStyle = CFS_EXCLUDE;
    candidate.ptCurrentPos.x = (LONG)lround(window->imeX * s);
    candidate.ptCurrentPos.y = (LONG)lround((window->imeY + window->imeHeight) * s);
    candidate.rcArea.left = (LONG)lround(window->imeX * s);
    candidate.rcArea.top = (LONG)lround(window->imeY * s);
    candidate.rcArea.right = (LONG)lround((window->imeX + window->imeWidth) * s);
    candidate.rcArea.bottom = (LONG)lround((window->imeY + window->imeHeight) * s);
    ImmSetCandidateWindow(context, &candidate);
    ImmReleaseContext(window->hwnd, context);
}

static bool handle_composition(Win32Window *window, LPARAM lParam) {
    HIMC context = ImmGetContext(window->hwnd);
    if (!context) return false;
    if (lParam & GCS_RESULTSTR) {
        LONG bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, NULL, 0);
        if (bytes > 0) {
            WCHAR *text = malloc((size_t)bytes + sizeof(WCHAR));
            if (!text) abort();
            ImmGetCompositionStringW(context, GCS_RESULTSTR, text, (DWORD)bytes);
            push_preedit(window, L"", 0, 0);
            push_commit(window, text, bytes / (LONG)sizeof(WCHAR));
            free(text);
        }
    }
    if (lParam & GCS_COMPSTR) {
        LONG bytes = ImmGetCompositionStringW(context, GCS_COMPSTR, NULL, 0);
        int caret = (lParam & GCS_CURSORPOS) ? (int)ImmGetCompositionStringW(context, GCS_CURSORPOS, NULL, 0) : -1;
        WCHAR *text = malloc((size_t)(bytes > 0 ? bytes : 0) + sizeof(WCHAR));
        if (!text) abort();
        if (bytes > 0) ImmGetCompositionStringW(context, GCS_COMPSTR, text, (DWORD)bytes);
        int count = bytes > 0 ? bytes / (LONG)sizeof(WCHAR) : 0;
        push_preedit(window, text, count, caret < 0 ? count : caret);
        free(text);
    }
    ImmReleaseContext(window->hwnd, context);
    return true;
}

// ---------------------------------------------------------------------------------------
// Pointer.

static uint32_t click(Win32Window *window, uint32_t button, int x, int y) {
    double now = silk_now();
    int cx = GetSystemMetrics(SM_CXDOUBLECLK) / 2, cy = GetSystemMetrics(SM_CYDOUBLECLK) / 2;
    bool same = window->lastClickButton == button &&
                now - window->lastClickTime < GetDoubleClickTime() / 1000.0 &&
                abs(x - window->lastClickX) <= cx && abs(y - window->lastClickY) <= cy;
    window->clickCount = same ? window->clickCount + 1 : 1;
    window->lastClickTime = now;
    window->lastClickX = x;
    window->lastClickY = y;
    window->lastClickButton = button;
    return window->clickCount;
}

static void push_button(Win32Window *window, bool down, uint32_t button, LPARAM lParam) {
    int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
    SilkWindowEvent event = silk_event(down ? SILK_EVENT_MOUSE_DOWN : SILK_EVENT_MOUSE_UP, window->id);
    event.code = button;
    event.x = x / window->scale;
    event.y = y / window->scale;
    event.modifiers = current_modifiers();
    if (down) {
        event.count = click(window, button, x, y);
        SetCapture(window->hwnd);
    } else if (!(GetKeyState(VK_LBUTTON) & 0x8000) && !(GetKeyState(VK_RBUTTON) & 0x8000) &&
               !(GetKeyState(VK_MBUTTON) & 0x8000)) {
        ReleaseCapture();
    }
    silk_queue_push(&event);
}

// Phase numbers, as src/bridge.silk's PHASES.
enum { PHASE_BEGAN = 1, PHASE_CHANGED = 2, PHASE_ENDED = 3, PHASE_CANCELLED = 4 };

// Turns a pointer message of a finger into a Touch event. Returns false for other pointers.
static bool push_touch(Win32Window *window, UINT message, WPARAM wParam) {
    UINT32 id = GET_POINTERID_WPARAM(wParam);
    POINTER_INPUT_TYPE type = PT_POINTER;
    if (!GetPointerType(id, &type) || type != PT_TOUCH) return false;
    POINTER_INFO info;
    if (!GetPointerInfo(id, &info)) return false;
    uint32_t phase = PHASE_CHANGED;
    if (message == WM_POINTERDOWN) {
        phase = PHASE_BEGAN;
    } else if (message == WM_POINTERUP) {
        phase = (info.pointerFlags & POINTER_FLAG_CANCELED) ? PHASE_CANCELLED : PHASE_ENDED;
    } else if (message == WM_POINTERCAPTURECHANGED) {
        phase = PHASE_CANCELLED;
    } else if (!(info.pointerFlags & POINTER_FLAG_INCONTACT)) {
        // A finger hovering over a screen that senses it: nothing is touched.
        return true;
    }
    POINT point = info.ptPixelLocation;
    ScreenToClient(window->hwnd, &point);
    SilkWindowEvent event = silk_event(SILK_EVENT_TOUCH, window->id);
    event.code = id;
    event.phase = phase;
    event.x = point.x / window->scale;
    event.y = point.y / window->scale;
    event.modifiers = current_modifiers();
    silk_queue_push(&event);
    return true;
}


// A cursor from `rgba` (width * height, not premultiplied), each pixel drawn `factor` times
// larger.
static HCURSOR pixel_cursor(const uint8_t *rgba, uint32_t width, uint32_t height, uint32_t hotX,
                            uint32_t hotY, uint32_t factor) {
    if (factor < 1) factor = 1;
    uint32_t w = width * factor, h = height * factor;
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = (LONG)w;
    info.bmiHeader.biHeight = -(LONG)h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP color = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!color || !bits) {
        if (color) DeleteObject(color);
        return NULL;
    }
    uint8_t *out = bits;
    for (uint32_t y = 0; y < h; y++)
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *p = rgba + ((size_t)(y / factor) * width + x / factor) * 4;
            uint8_t *q = out + ((size_t)y * w + x) * 4;
            q[0] = p[2];
            q[1] = p[1];
            q[2] = p[0];
            q[3] = p[3];
        }
    HBITMAP mask = CreateBitmap((int)w, (int)h, 1, 1, NULL);
    ICONINFO icon = {FALSE, hotX * factor, hotY * factor, mask, color};
    HCURSOR result = (HCURSOR)CreateIconIndirect(&icon);
    DeleteObject(mask);
    DeleteObject(color);
    return result;
}

// The bundled image of `cursor` at `factor`, made once.
static HCURSOR bundled_cursor(uint32_t cursor, uint32_t factor) {
    static HCURSOR made[40][4];
    if (cursor >= 40) return NULL;
    if (factor < 1) factor = 1;
    if (factor > 4) factor = 4;
    if (made[cursor][factor - 1]) return made[cursor][factor - 1];
    uint8_t rgba[SILK_CURSOR_ART * SILK_CURSOR_ART * 4];
    uint32_t hotX, hotY;
    if (!silk_cursor_bundled(cursor, 1, rgba, &hotX, &hotY)) return NULL;
    made[cursor][factor - 1] = pixel_cursor(rgba, SILK_CURSOR_ART, SILK_CURSOR_ART, hotX, hotY, factor);
    return made[cursor][factor - 1];
}

static uint32_t cursor_factor(Win32Window *window) {
    UINT dpi = dpi_of(window->hwnd);
    uint32_t factor = (dpi + 48) / 96;
    return factor < 1 ? 1 : factor;
}

static HCURSOR cursor_handle(Win32Window *window) {
    if (window->custom) return window->custom;
    uint32_t cursor = window->cursor;
    // Windows has no grab, grabbing, column, row, vertical text, copy, alias, context menu,
    // zoom, cell or no-drop cursor: those are bundled images.
    switch (cursor) {
    case 4: case 5: case 15: case 16: case 17: case 18: case 19: case 20: case 25: case 26:
    case 27: case 28: {
        HCURSOR bundled = bundled_cursor(cursor, cursor_factor(window));
        if (bundled) return bundled;
        break;
    }
    default: break;
    }
    static const WORD ids[] = {
        32512, // Arrow IDC_ARROW
        32513, // IBeam
        32649, // PointingHand IDC_HAND
        32515, // Crosshair
        32646, // ClosedHand: IDC_SIZEALL
        32649, // OpenHand: IDC_HAND
        32648, // NotAllowed IDC_NO
        32644, // ResizeLeftRight IDC_SIZEWE
        32645, // ResizeUpDown IDC_SIZENS
        32644, 32644, 32645, 32645,
        32642, // NWSE IDC_SIZENWSE
        32643, // NESW IDC_SIZENESW
        32644, 32645,
        32513, // IBeamVertical
        32512, 32512, 32512,
        32512, // None, handled below
        32514, // Wait IDC_WAIT
        32650, // Progress IDC_APPSTARTING
        32651, // Help IDC_HELP
        32512, 32512, // ZoomIn, ZoomOut
        32515, // Cell
        32648, // NoDrop
        32646, 32646, // Move, AllScroll IDC_SIZEALL
        32642, 32643, 32643, 32642, // ResizeUpLeft, UpRight, DownLeft, DownRight
    };
    if (cursor == 21) return NULL;
    if (cursor >= sizeof ids / sizeof ids[0]) cursor = 0;
    return LoadCursorW(NULL, MAKEINTRESOURCEW(ids[cursor]));
}

// ---------------------------------------------------------------------------------------
// Window state.

static void push_resized(Win32Window *window) {
    SilkWindowEvent event = silk_event(SILK_EVENT_RESIZED, window->id);
    event.x = window->pixelWidth / window->scale;
    event.y = window->pixelHeight / window->scale;
    event.value = window->scale;
    silk_queue_push(&event);
    window->frame.pending = true;
    window->frame.forced = true;
}

static void set_full_screen(Win32Window *window, bool full) {
    if (full == window->fullScreen) return;
    HWND hwnd = window->hwnd;
    if (full) {
        window->placement.length = sizeof window->placement;
        GetWindowPlacement(hwnd, &window->placement);
        window->savedStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
        MONITORINFO monitor;
        monitor.cbSize = sizeof monitor;
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
        SetWindowLongPtrW(hwnd, GWL_STYLE, (window->savedStyle & ~(LONG_PTR)WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetMenu(hwnd, NULL);
        SetWindowPos(hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(hwnd, GWL_STYLE, window->savedStyle);
        SetMenu(hwnd, window->menu);
        SetWindowPlacement(hwnd, &window->placement);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    window->fullScreen = full;
    SilkWindowEvent event = silk_event(SILK_EVENT_FULL_SCREEN_CHANGED, window->id);
    if (full) event.flags |= SILK_FLAG_ON;
    silk_queue_push(&event);
}

static void system_item(Win32Window *window, uint32_t item) {
    SilkWindowEvent event;
    switch (item) {
    case SILK_ITEM_QUIT:
        event = silk_event(SILK_EVENT_QUIT_REQUESTED, 0);
        silk_queue_push(&event);
        break;
    case SILK_ITEM_CLOSE_WINDOW:
        if (window) {
            event = silk_event(SILK_EVENT_CLOSE_REQUESTED, window->id);
            silk_queue_push(&event);
        }
        break;
    case SILK_ITEM_MINIMIZE:
        if (window) ShowWindow(window->hwnd, SW_MINIMIZE);
        break;
    case SILK_ITEM_ZOOM:
        if (window) ShowWindow(window->hwnd, IsZoomed(window->hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        break;
    case SILK_ITEM_TOGGLE_FULL_SCREEN:
        if (window) set_full_screen(window, !window->fullScreen);
        break;
    case SILK_ITEM_UNDO:
    case SILK_ITEM_REDO:
    case SILK_ITEM_CUT:
    case SILK_ITEM_COPY:
    case SILK_ITEM_PASTE:
    case SILK_ITEM_SELECT_ALL:
        // The shortcuts reach the window as keys; a click on the item sends the command.
        if (window) {
            event = silk_event(SILK_EVENT_EDIT_COMMAND, window->id);
            event.code = item - SILK_ITEM_UNDO;
            silk_queue_push(&event);
        }
        break;
    default:
        break;
    }
}

static void run_task(Task *task);
static void hotkey_pressed(int hotkey);
static HWND trayWindow;
static LRESULT tray_message(UINT message, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (hwnd && hwnd == trayWindow) {
        LRESULT result = tray_message(message, wParam, lParam);
        yield_if_ready();
        return result;
    }
    if (hwnd == helper) {
        if (message == WM_SILK_RUN) {
            run_task((Task *)lParam);
            yield_if_ready();
            return 0;
        }
        if (message == WM_SILK_WAKE) {
            yield_if_ready();
            return 0;
        }
        if (message == WM_HOTKEY) {
            hotkey_pressed((int)wParam);
            yield_if_ready();
            return 0;
        }
        if (message == WM_SILK_KEYSTATE) {
            BYTE *state = (BYTE *)lParam;
            SetKeyboardState(state);
            free(state);
            return 0;
        }
        if (message == WM_SETTINGCHANGE && lParam &&
            wcscmp((const WCHAR *)lParam, L"ImmersiveColorSet") == 0) {
            uint32_t value = read_appearance();
            if (value != appearanceValue) {
                appearanceValue = value;
                for (size_t i = 0; i < windowCount; i++)
                    if (windows[i]->open) apply_dark_title(windows[i]->hwnd);
                SilkWindowEvent event = silk_event(SILK_EVENT_APPEARANCE_CHANGED, 0);
                event.code = value;
                silk_queue_push(&event);
                yield_if_ready();
            }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    Win32Window *window = find_hwnd(hwnd);
    if (!window) return DefWindowProcW(hwnd, message, wParam, lParam);
    LRESULT result = 0;
    bool handled = true;
    switch (message) {
    case WM_CLOSE: {
        SilkWindowEvent event = silk_event(SILK_EVENT_CLOSE_REQUESTED, window->id);
        silk_queue_push(&event);
        break;
    }
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            if (!window->minimized) {
                window->minimized = true;
                SilkWindowEvent event = silk_event(SILK_EVENT_OCCLUDED, window->id);
                event.flags |= SILK_FLAG_ON;
                silk_queue_push(&event);
            }
            break;
        }
        bool restored = window->minimized;
        window->minimized = false;
        int width = LOWORD(lParam), height = HIWORD(lParam);
        if (restored) {
            SilkWindowEvent event = silk_event(SILK_EVENT_OCCLUDED, window->id);
            silk_queue_push(&event);
        }
        if (width != window->pixelWidth || height != window->pixelHeight || restored) {
            window->pixelWidth = width;
            window->pixelHeight = height;
            push_resized(window);
        }
        break;
    }
    case WM_MOVE: {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        SilkWindowEvent event = silk_event(SILK_EVENT_MOVED, window->id);
        event.x = rect.left / window->scale;
        event.y = rect.top / window->scale;
        silk_queue_push(&event);
        break;
    }
    case WM_DPICHANGED: {
        double scale = HIWORD(wParam) / 96.0;
        RECT *suggested = (RECT *)lParam;
        if (fabs(scale - window->scale) > 1e-9) {
            window->scale = scale;
            SilkWindowEvent event = silk_event(SILK_EVENT_SCALE_FACTOR_CHANGED, window->id);
            event.value = scale;
            silk_queue_push(&event);
        }
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        RECT client;
        GetClientRect(hwnd, &client);
        window->pixelWidth = client.right;
        window->pixelHeight = client.bottom;
        push_resized(window);
        break;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = (MINMAXINFO *)lParam;
        if (window->minWidth > 0 || window->minHeight > 0) {
            RECT rect;
            client_rect_for(window, (int)ceil(window->minWidth * window->scale),
                            (int)ceil(window->minHeight * window->scale), &rect);
            info->ptMinTrackSize.x = rect.right - rect.left;
            info->ptMinTrackSize.y = rect.bottom - rect.top;
        }
        break;
    }
    case WM_ENTERSIZEMOVE:
    case WM_ENTERMENULOOP:
        // The system's modal loop does not return to the pump; a timer keeps it waking.
        SetTimer(hwnd, TIMER_MODAL, USER_TIMER_MINIMUM, NULL);
        break;
    case WM_EXITSIZEMOVE:
    case WM_EXITMENULOOP:
        KillTimer(hwnd, TIMER_MODAL);
        break;
    case WM_TIMER:
        if (wParam == TIMER_MODAL) check_vblank();
        else handled = false;
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        window->frame.pending = true;
        window->frame.forced = true;
        break;
    }
    case WM_ERASEBKGND:
        result = 1;
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS: {
        bool focused = message == WM_SETFOCUS;
        window->focused = focused;
        SilkWindowEvent event = silk_event(SILK_EVENT_FOCUSED, window->id);
        if (focused) event.flags |= SILK_FLAG_ON;
        silk_queue_push(&event);
        if (focused) place_ime(window);
        break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        handle_key(window, message, wParam, lParam);
        // Alt+F4 and the menu bar's Alt and F10 stay with the system.
        handled = !(message == WM_SYSKEYDOWN || message == WM_SYSKEYUP);
        break;
    case WM_CHAR:
        handle_char(window, wParam);
        break;
    case WM_SYSCHAR:
        handled = window->menu != NULL ? false : true;
        break;
    case WM_SYSCOMMAND:
        // Without a menu bar, Alt alone would enter the window menu's modal mode.
        if ((wParam & 0xFFF0) == SC_KEYMENU && !window->menu && lParam == 0) break;
        handled = false;
        break;
    case WM_IME_SETCONTEXT:
        lParam &= ~(LPARAM)ISC_SHOWUICOMPOSITIONWINDOW;
        result = DefWindowProcW(hwnd, message, wParam, lParam);
        break;
    case WM_IME_STARTCOMPOSITION:
        place_ime(window);
        break;
    case WM_IME_COMPOSITION:
        if (!handle_composition(window, lParam)) handled = false;
        break;
    case WM_IME_ENDCOMPOSITION:
        push_preedit(window, L"", 0, 0);
        break;
    case WM_MOUSEMOVE: {
        if (!window->tracking) {
            TRACKMOUSEEVENT track = {sizeof track, TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&track);
            window->tracking = true;
            SilkWindowEvent entered = silk_event(SILK_EVENT_MOUSE_ENTERED, window->id);
            entered.x = (short)LOWORD(lParam) / window->scale;
            entered.y = (short)HIWORD(lParam) / window->scale;
            silk_queue_push(&entered);
        }
        SilkWindowEvent event = silk_event(SILK_EVENT_MOUSE_MOVED, window->id);
        event.x = (short)LOWORD(lParam) / window->scale;
        event.y = (short)HIWORD(lParam) / window->scale;
        event.modifiers = current_modifiers();
        silk_queue_push(&event);
        break;
    }
    case WM_MOUSELEAVE: {
        window->tracking = false;
        SilkWindowEvent event = silk_event(SILK_EVENT_MOUSE_EXITED, window->id);
        silk_queue_push(&event);
        break;
    }
    case WM_LBUTTONDOWN: push_button(window, true, SILK_BUTTON_LEFT, lParam); break;
    case WM_LBUTTONUP: push_button(window, false, SILK_BUTTON_LEFT, lParam); break;
    case WM_RBUTTONDOWN: push_button(window, true, SILK_BUTTON_RIGHT, lParam); break;
    case WM_RBUTTONUP: push_button(window, false, SILK_BUTTON_RIGHT, lParam); break;
    case WM_MBUTTONDOWN: push_button(window, true, SILK_BUTTON_MIDDLE, lParam); break;
    case WM_MBUTTONUP: push_button(window, false, SILK_BUTTON_MIDDLE, lParam); break;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
        push_button(window, message == WM_XBUTTONDOWN,
                    HIWORD(wParam) == XBUTTON1 ? SILK_BUTTON_BACK : SILK_BUTTON_FORWARD, lParam);
        result = TRUE;
        break;
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    case WM_POINTERCAPTURECHANGED:
        // Fingers become Touch events. Handling their pointer messages keeps Windows from
        // also sending them as mouse messages; mouse and pen go on to DefWindowProc.
        handled = push_touch(window, message, wParam);
        break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        POINT point = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
        ScreenToClient(hwnd, &point);
        double delta = (short)HIWORD(wParam) / (double)WHEEL_DELTA;
        SilkWindowEvent event = silk_event(SILK_EVENT_SCROLL, window->id);
        event.x = point.x / window->scale;
        event.y = point.y / window->scale;
        event.modifiers = current_modifiers();
        // Wheel away from the user shows what is above; tilting right shows what is right.
        if (message == WM_MOUSEWHEEL) event.dy = delta;
        else event.dx = -delta;
        silk_queue_push(&event);
        break;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(cursor_handle(window));
            result = TRUE;
        } else {
            handled = false;
        }
        break;
    case WM_COMMAND:
        if (HIWORD(wParam) == 0 && LOWORD(wParam) >= MENU_ACTION_BASE) {
            SilkWindowEvent event = silk_event(SILK_EVENT_MENU_ACTION, 0);
            event.code = LOWORD(wParam) - MENU_ACTION_BASE;
            silk_queue_push(&event);
        } else if (HIWORD(wParam) == 0 && LOWORD(wParam) >= MENU_SYSTEM_BASE) {
            system_item(window, LOWORD(wParam) - MENU_SYSTEM_BASE);
        } else {
            handled = false;
        }
        break;
    case WM_NCCALCSIZE:
        // A hidden title bar: the content takes the whole window, the resize borders stay.
        if (window->titleHidden && wParam && !window->fullScreen) {
            if (IsZoomed(hwnd)) {
                NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS *)lParam;
                int frame = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                params->rgrc[0].left += frame;
                params->rgrc[0].right -= frame;
                params->rgrc[0].top += frame;
                params->rgrc[0].bottom -= frame;
            }
            result = 0;
        } else {
            handled = false;
        }
        break;
    default:
        handled = false;
        break;
    }
    if (!handled) result = DefWindowProcW(hwnd, message, wParam, lParam);
    yield_if_ready();
    return result;
}

// ---------------------------------------------------------------------------------------
// Files dragged in from other apps: an OLE drop target per window.

typedef struct SilkDropTarget {
    IDropTarget target;
    LONG references;
    uint32_t window;
    bool files; // the drag carries CF_HDROP
} SilkDropTarget;

static FORMATETC hdrop_format(void) {
    FORMATETC format = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return format;
}

// The dragged paths, in new memory freed with free_texts.
static size_t drop_paths(IDataObject *data, SilkText *texts, size_t capacity) {
    FORMATETC format = hdrop_format();
    STGMEDIUM medium;
    if (FAILED(IDataObject_GetData(data, &format, &medium))) return 0;
    size_t count = 0;
    HDROP drop = (HDROP)GlobalLock(medium.hGlobal);
    if (drop) {
        UINT files = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < files && count < capacity; i++) {
            UINT length = DragQueryFileW(drop, i, NULL, 0);
            WCHAR *path = malloc((length + 1) * sizeof *path);
            if (!path) break;
            DragQueryFileW(drop, i, path, length + 1);
            texts[count].bytes = narrow(path, (int)length, &texts[count].length);
            free(path);
            if (texts[count].bytes) count++;
        }
        GlobalUnlock(medium.hGlobal);
    }
    ReleaseStgMedium(&medium);
    return count;
}

static void free_texts(SilkText *texts, size_t count) {
    for (size_t i = 0; i < count; i++) free(texts[i].bytes);
}

static void drop_point(uint32_t id, POINTL screen, double *x, double *y) {
    Win32Window *window = find(id);
    POINT point = {screen.x, screen.y};
    *x = *y = 0;
    if (!window) return;
    ScreenToClient(window->hwnd, &point);
    *x = point.x / window->scale;
    *y = point.y / window->scale;
}

static HRESULT STDMETHODCALLTYPE drop_query(IDropTarget *self, REFIID riid, void **out) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) {
        *out = self;
        IDropTarget_AddRef(self);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE drop_add_ref(IDropTarget *self) {
    return (ULONG)InterlockedIncrement(&((SilkDropTarget *)self)->references);
}

static ULONG STDMETHODCALLTYPE drop_release(IDropTarget *self) {
    LONG left = InterlockedDecrement(&((SilkDropTarget *)self)->references);
    if (left == 0) free(self);
    return (ULONG)left;
}

static HRESULT STDMETHODCALLTYPE drop_enter(IDropTarget *self, IDataObject *data, DWORD keys,
                                            POINTL point, DWORD *effect) {
    SilkDropTarget *target = (SilkDropTarget *)self;
    FORMATETC format = hdrop_format();
    target->files = SUCCEEDED(IDataObject_QueryGetData(data, &format));
    *effect = target->files ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    if (!target->files) return S_OK;
    SilkText paths[SILK_MAX_TEXTS];
    size_t count = drop_paths(data, paths, SILK_MAX_TEXTS);
    double x, y;
    drop_point(target->window, point, &x, &y);
    silk_push_drag(SILK_EVENT_FILE_DRAG_ENTERED, target->window, x, y, paths, count);
    free_texts(paths, count);
    yield_if_ready();
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE drop_over(IDropTarget *self, DWORD keys, POINTL point,
                                           DWORD *effect) {
    SilkDropTarget *target = (SilkDropTarget *)self;
    *effect = target->files ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    if (!target->files) return S_OK;
    double x, y;
    drop_point(target->window, point, &x, &y);
    silk_push_drag(SILK_EVENT_FILE_DRAG_MOVED, target->window, x, y, NULL, 0);
    yield_if_ready();
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE drop_leave(IDropTarget *self) {
    SilkDropTarget *target = (SilkDropTarget *)self;
    if (!target->files) return S_OK;
    target->files = false;
    silk_push_drag(SILK_EVENT_FILE_DRAG_LEFT, target->window, 0, 0, NULL, 0);
    yield_if_ready();
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE drop_drop(IDropTarget *self, IDataObject *data, DWORD keys,
                                           POINTL point, DWORD *effect) {
    SilkDropTarget *target = (SilkDropTarget *)self;
    *effect = target->files ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    if (!target->files) return S_OK;
    target->files = false;
    SilkText paths[SILK_MAX_TEXTS];
    size_t count = drop_paths(data, paths, SILK_MAX_TEXTS);
    double x, y;
    drop_point(target->window, point, &x, &y);
    silk_push_drag(SILK_EVENT_FILE_DROPPED, target->window, x, y, paths, count);
    free_texts(paths, count);
    yield_if_ready();
    return S_OK;
}

static IDropTargetVtbl dropTargetMethods = {
    drop_query, drop_add_ref, drop_release, drop_enter, drop_over, drop_leave, drop_drop,
};

static IDropTarget *new_drop_target(uint32_t window) {
    SilkDropTarget *target = calloc(1, sizeof *target);
    if (!target) return NULL;
    target->target.lpVtbl = &dropTargetMethods;
    target->references = 1;
    target->window = window;
    return &target->target;
}

// ---------------------------------------------------------------------------------------
// Windows.

uint32_t silk_window_open(const uint8_t *title, size_t titleLength, double width, double height,
                          double minWidth, double minHeight, uint32_t flags) {
    if (!started || windowCount == sizeof windows / sizeof windows[0]) return 0;
    Win32Window *window = calloc(1, sizeof *window);
    if (!window) return 0;
    window->id = nextWindow++;
    window->open = true;
    window->resizable = (flags & SILK_WINDOW_RESIZABLE) != 0;
    window->titleHidden = (flags & SILK_WINDOW_TITLE_HIDDEN) != 0;
    window->minWidth = minWidth;
    window->minHeight = minHeight;
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!window->resizable) style &= ~(DWORD)(WS_THICKFRAME | WS_MAXIMIZEBOX);
    DWORD exStyle = WS_EX_APPWINDOW;
    if (flags & SILK_WINDOW_TRANSPARENT) exStyle |= WS_EX_NOREDIRECTIONBITMAP;
    UINT dpi = dpi_of(NULL);
    window->scale = dpi / 96.0;
    WCHAR *wideTitle = wide(title, titleLength);
    windows[windowCount++] = window;
    HWND hwnd = CreateWindowExW(exStyle, L"SilkWindow", wideTitle, style, CW_USEDEFAULT,
                                CW_USEDEFAULT, 100, 100, NULL, NULL, instance, NULL);
    free(wideTitle);
    if (!hwnd) {
        windowCount--;
        free(window);
        return 0;
    }
    window->hwnd = hwnd;
    window->scale = dpi_of(hwnd) / 96.0;
    window->pixelWidth = (int)lround(width * window->scale);
    window->pixelHeight = (int)lround(height * window->scale);
    apply_dark_title(hwnd);
    window->menu = window->titleHidden ? NULL : build_menu();
    if (window->menu) SetMenu(hwnd, window->menu);
    if (window->titleHidden) SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    RECT rect;
    client_rect_for(window, window->pixelWidth, window->pixelHeight, &rect);
    SetWindowPos(hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RECT client;
    GetClientRect(hwnd, &client);
    window->pixelWidth = client.right;
    window->pixelHeight = client.bottom;
    // Input methods start off, as on the other backends, until setIme turns them on.
    window->savedContext = ImmAssociateContext(hwnd, NULL);
    window->dropTarget = new_drop_target(window->id);
    if (window->dropTarget && FAILED(RegisterDragDrop(hwnd, window->dropTarget))) {
        IDropTarget_Release(window->dropTarget);
        window->dropTarget = NULL;
    }
    if (flags & SILK_WINDOW_VISIBLE) silk_window_set_visible(window->id, 1);
    return window->id;
}

void silk_window_close(uint32_t id) {
    Win32Window *window = find(id);
    if (!window) return;
    window->open = false;
    if (window->savedContext) ImmAssociateContext(window->hwnd, window->savedContext);
    if (window->dropTarget) {
        RevokeDragDrop(window->hwnd);
        IDropTarget_Release(window->dropTarget);
        window->dropTarget = NULL;
    }
    DestroyWindow(window->hwnd);
    if (window->custom) DestroyCursor(window->custom);
    window->custom = NULL;
    if (window->menu) DestroyMenu(window->menu);
    silk_queue_forget_window(id);
}

void silk_window_request_frame(uint32_t id) {
    Win32Window *window = find(id);
    if (!window || window->frame.pending) return;
    window->frame.pending = true;
    SetEvent(wantVblank);
}

void silk_window_content_size(uint32_t id, double *width, double *height) {
    *width = *height = 0;
    Win32Window *window = find(id);
    if (!window) return;
    *width = window->pixelWidth / window->scale;
    *height = window->pixelHeight / window->scale;
}

double silk_window_scale_factor(uint32_t id) {
    Win32Window *window = find(id);
    return window ? window->scale : 1.0;
}

uint32_t silk_window_raw_surface(uint32_t id, void **first, void **second, uint64_t *xid) {
    *first = *second = NULL;
    *xid = 0;
    Win32Window *window = find(id);
    if (!window) return SILK_SURFACE_NONE;
    *first = window->hwnd;
    *second = instance;
    return SILK_SURFACE_WIN32;
}

void silk_window_set_title(uint32_t id, const uint8_t *title, size_t length) {
    Win32Window *window = find(id);
    if (!window) return;
    WCHAR *text = wide(title, length);
    SetWindowTextW(window->hwnd, text);
    free(text);
}

void silk_window_set_content_size(uint32_t id, double width, double height) {
    Win32Window *window = find(id);
    if (!window) return;
    RECT rect;
    client_rect_for(window, (int)lround(width * window->scale), (int)lround(height * window->scale), &rect);
    SetWindowPos(window->hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void silk_window_set_min_content_size(uint32_t id, double width, double height) {
    Win32Window *window = find(id);
    if (!window) return;
    window->minWidth = width;
    window->minHeight = height;
}

void silk_window_set_visible(uint32_t id, uint8_t visible) {
    Win32Window *window = find(id);
    if (!window || window->visible == (visible != 0)) return;
    window->visible = visible != 0;
    ShowWindow(window->hwnd, visible ? SW_SHOW : SW_HIDE);
    if (visible) {
        UpdateWindow(window->hwnd);
        push_resized(window);
    } else {
        window->frame.pending = false;
    }
}

void silk_window_focus(uint32_t id) {
    Win32Window *window = find(id);
    if (!window) return;
    SetForegroundWindow(window->hwnd);
    SetFocus(window->hwnd);
}

void silk_window_minimize(uint32_t id) {
    Win32Window *window = find(id);
    if (window) ShowWindow(window->hwnd, SW_MINIMIZE);
}

void silk_window_toggle_full_screen(uint32_t id) {
    Win32Window *window = find(id);
    if (window) set_full_screen(window, !window->fullScreen);
}

static void post_task(Task *task) {
    if (fiberMode) PostMessageW(helper, WM_SILK_RUN, 0, (LPARAM)task);
    else run_task(task);
}

void silk_window_start_window_drag(uint32_t id) {
    Win32Window *window = find(id);
    if (!window) return;
    Task *task = calloc(1, sizeof *task);
    if (!task) return;
    task->kind = 3;
    task->window = id;
    post_task(task);
}

static void show_cursor(Win32Window *window) {
    POINT point;
    GetCursorPos(&point);
    if (WindowFromPoint(point) == window->hwnd) {
        ScreenToClient(window->hwnd, &point);
        RECT client;
        GetClientRect(window->hwnd, &client);
        if (PtInRect(&client, point)) SetCursor(cursor_handle(window));
    }
}

void silk_window_set_cursor(uint32_t id, uint32_t cursor) {
    Win32Window *window = find(id);
    if (!window) return;
    window->cursor = cursor;
    HCURSOR old = window->custom;
    window->custom = NULL;
    show_cursor(window);
    if (old) DestroyCursor(old);
}

uint8_t silk_window_set_cursor_image(uint32_t id, const uint8_t *pixels, uint32_t width,
                                     uint32_t height, uint32_t hotX, uint32_t hotY, double scale) {
    Win32Window *window = find(id);
    if (!window) return 0;
    double ratio = cursor_factor(window) / (scale > 0 ? scale : 1.0);
    HCURSOR cursor = pixel_cursor(pixels, width, height, hotX, hotY, ratio >= 1.5 ? (uint32_t)lround(ratio) : 1);
    if (!cursor) return 0;
    HCURSOR old = window->custom;
    window->custom = cursor;
    show_cursor(window);
    if (old) DestroyCursor(old);
    return 1;
}

void silk_window_set_ime(uint32_t id, uint8_t enabled, double x, double y, double width,
                         double height) {
    Win32Window *window = find(id);
    if (!window) return;
    window->imeX = x;
    window->imeY = y;
    window->imeWidth = width;
    window->imeHeight = height;
    if ((enabled != 0) != window->imeEnabled) {
        window->imeEnabled = enabled != 0;
        if (enabled) {
            ImmAssociateContext(window->hwnd, window->savedContext);
        } else {
            HIMC context = ImmGetContext(window->hwnd);
            if (context) {
                ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
                ImmReleaseContext(window->hwnd, context);
            }
            window->savedContext = ImmAssociateContext(window->hwnd, NULL);
        }
    }
    if (enabled) place_ime(window);
}

// ---------------------------------------------------------------------------------------
// File dialogs (IFileDialog), run on the pump fiber so events keep flowing while open.

static void push_dialog_result(uint32_t id, SilkText *texts, size_t count) {
    SilkWindowEvent event = silk_event(SILK_EVENT_DIALOG_CLOSED, 0);
    event.code = id;
    event.count = (uint32_t)count;
    silk_queue_push_texts(&event, texts, count);
}

static void add_item_path(IShellItem *item, SilkText *texts, size_t *count) {
    WCHAR *path = NULL;
    if (*count < SILK_MAX_TEXTS && SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path))) {
        texts[*count].bytes = narrow(path, -1, &texts[*count].length);
        (*count)++;
        CoTaskMemFree(path);
    }
}

static void run_dialog(Task *task) {
    bool save = task->kind == 2;
    IFileDialog *dialog = NULL;
    HRESULT hr = CoCreateInstance(save ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog, NULL,
                                  CLSCTX_INPROC_SERVER,
                                  save ? &IID_IFileSaveDialog : &IID_IFileOpenDialog, (void **)&dialog);
    SilkText texts[SILK_MAX_TEXTS];
    size_t count = 0;
    if (SUCCEEDED(hr)) {
        FILEOPENDIALOGOPTIONS options = 0;
        IFileDialog_GetOptions(dialog, &options);
        options |= FOS_FORCEFILESYSTEM;
        if (!save) {
            if ((task->flags & 2) && !(task->flags & 1)) options |= FOS_PICKFOLDERS;
            if (task->flags & 4) options |= FOS_ALLOWMULTISELECT;
            options |= FOS_FILEMUSTEXIST;
        } else {
            options |= FOS_OVERWRITEPROMPT;
        }
        IFileDialog_SetOptions(dialog, options);
        if (task->title && task->title[0]) IFileDialog_SetTitle(dialog, task->title);
        if (save && task->name && task->name[0]) IFileDialog_SetFileName(dialog, task->name);
        if (task->directory && task->directory[0]) {
            IShellItem *folder = NULL;
            if (SUCCEEDED(SHCreateItemFromParsingName(task->directory, NULL, &IID_IShellItem, (void **)&folder))) {
                IFileDialog_SetFolder(dialog, folder);
                IShellItem_Release(folder);
            }
        }
        size_t extensionCount = 0;
        for (char **p = task->extensions; p && *p; p++) extensionCount++;
        WCHAR pattern[2048] = {0};
        if (extensionCount && !(options & FOS_PICKFOLDERS)) {
            for (size_t i = 0; i < extensionCount; i++) {
                WCHAR one[128];
                MultiByteToWideChar(CP_UTF8, 0, task->extensions[i], -1, one, 120);
                if (i) wcsncat(pattern, L";", 2047 - wcslen(pattern));
                wcsncat(pattern, L"*.", 2047 - wcslen(pattern));
                wcsncat(pattern, one, 2047 - wcslen(pattern));
            }
            COMDLG_FILTERSPEC spec[2] = {{pattern, pattern}, {L"All files", L"*.*"}};
            IFileDialog_SetFileTypes(dialog, save ? 1 : 2, spec);
            if (save) {
                WCHAR first[128];
                MultiByteToWideChar(CP_UTF8, 0, task->extensions[0], -1, first, 120);
                IFileDialog_SetDefaultExtension(dialog, first);
            }
        }
        Win32Window *parent = find(task->window);
        hr = IFileDialog_Show(dialog, parent ? parent->hwnd : NULL);
        if (SUCCEEDED(hr)) {
            if (!save && (options & FOS_ALLOWMULTISELECT)) {
                IShellItemArray *items = NULL;
                if (SUCCEEDED(IFileOpenDialog_GetResults((IFileOpenDialog *)dialog, &items))) {
                    DWORD n = 0;
                    IShellItemArray_GetCount(items, &n);
                    for (DWORD i = 0; i < n; i++) {
                        IShellItem *item = NULL;
                        if (SUCCEEDED(IShellItemArray_GetItemAt(items, i, &item))) {
                            add_item_path(item, texts, &count);
                            IShellItem_Release(item);
                        }
                    }
                    IShellItemArray_Release(items);
                }
            } else {
                IShellItem *item = NULL;
                if (SUCCEEDED(IFileDialog_GetResult(dialog, &item))) {
                    add_item_path(item, texts, &count);
                    IShellItem_Release(item);
                }
            }
        }
        IFileDialog_Release(dialog);
    }
    push_dialog_result(task->id, texts, count);
    for (size_t i = 0; i < count; i++) free(texts[i].bytes);
}

static void run_context_menu(Task *task) {
    Win32Window *window = find(task->window);
    UINT command = 0;
    if (window) {
        command = (UINT)TrackPopupMenu(task->menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                       task->point.x, task->point.y, 0, window->hwnd, NULL);
    }
    DestroyMenu(task->menu);
    if (command >= MENU_ACTION_BASE) {
        SilkWindowEvent event = silk_event(SILK_EVENT_MENU_ACTION, 0);
        event.code = command - MENU_ACTION_BASE;
        silk_queue_push(&event);
    } else if (command >= MENU_SYSTEM_BASE) {
        system_item(window, command - MENU_SYSTEM_BASE);
    } else {
        SilkWindowEvent event = silk_event(SILK_EVENT_CONTEXT_MENU_CLOSED, task->window);
        silk_queue_push(&event);
    }
}

uint8_t silk_window_context_menu_commit(uint32_t id, double x, double y) {
    silk_context_commit_pending();
    Win32Window *window = find(id);
    if (!window) return 0;
    Task *task = calloc(1, sizeof *task);
    if (!task) return 0;
    task->menu = build_menu_from(silk_context_entries, silk_context_count, true);
    if (!task->menu) {
        free(task);
        return 0;
    }
    task->kind = 4;
    task->window = id;
    task->point.x = (LONG)lround(x * window->scale);
    task->point.y = (LONG)lround(y * window->scale);
    ClientToScreen(window->hwnd, &task->point);
    post_task(task);
    return 1;
}

// Taskbar badge and progress, through ITaskbarList3 on every window's button.
static ITaskbarList3 *taskbar;
static HICON badgeIcon;
static double progressValue = -1;

static ITaskbarList3 *taskbar_list(void) {
    if (taskbar) return taskbar;
    ITaskbarList3 *list = NULL;
    if (FAILED(CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList3,
                                (void **)&list)))
        return NULL;
    if (FAILED(ITaskbarList3_HrInit(list))) {
        ITaskbarList3_Release(list);
        return NULL;
    }
    taskbar = list;
    return taskbar;
}

static HICON badge_icon(const WCHAR *label) {
    enum { SIZE = 32 };
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = SIZE;
    info.bmiHeader.biHeight = -SIZE;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP color = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dc || !color || !bits) {
        if (color) DeleteObject(color);
        if (dc) DeleteDC(dc);
        return NULL;
    }
    HGDIOBJ old = SelectObject(dc, color);
    HBRUSH red = CreateSolidBrush(RGB(220, 38, 38));
    HGDIOBJ oldBrush = SelectObject(dc, red);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, 0, 0, SIZE + 1, SIZE + 1);
    if (label[0]) {
        HFONT font = CreateFontW(label[2] ? -14 : -20, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0,
                                 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT rect = {0, 0, SIZE, SIZE};
        DrawTextW(dc, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(red);
    SelectObject(dc, old);
    // GDI leaves alpha at zero: painted pixels become opaque, the rest stays clear.
    uint32_t *pixels = bits;
    for (int i = 0; i < SIZE * SIZE; i++)
        if (pixels[i]) pixels[i] |= 0xff000000u;
    HBITMAP mask = CreateBitmap(SIZE, SIZE, 1, 1, NULL);
    ICONINFO icon = {TRUE, 0, 0, mask, color};
    HICON result = CreateIconIndirect(&icon);
    DeleteObject(mask);
    DeleteObject(color);
    DeleteDC(dc);
    return result;
}

static void apply_taskbar(Win32Window *window) {
    ITaskbarList3 *list = taskbar_list();
    if (!list) return;
    ITaskbarList3_SetOverlayIcon(list, window->hwnd, badgeIcon, NULL);
    if (progressValue < 0) {
        ITaskbarList3_SetProgressState(list, window->hwnd, TBPF_NOPROGRESS);
    } else {
        ITaskbarList3_SetProgressState(list, window->hwnd, TBPF_NORMAL);
        ITaskbarList3_SetProgressValue(list, window->hwnd, (ULONGLONG)lround(progressValue * 1000), 1000);
    }
}

uint8_t silk_window_set_badge(const uint8_t *text, size_t length) {
    if (!taskbar_list()) return 0;
    WCHAR label[4] = {0};
    bool digits = length > 0;
    for (size_t i = 0; i < length; i++)
        if (text[i] < '0' || text[i] > '9') digits = false;
    if (digits) {
        size_t start = 0;
        while (start + 1 < length && text[start] == '0') start++;
        if (length - start > 2) {
            wcscpy(label, L"99+");
        } else {
            for (size_t i = start; i < length; i++) label[i - start] = text[i];
        }
    }
    if (badgeIcon) DestroyIcon(badgeIcon);
    badgeIcon = length ? badge_icon(label) : NULL;
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->open) apply_taskbar(windows[i]);
    return 1;
}

uint8_t silk_window_set_progress(double fraction) {
    if (!taskbar_list()) return 0;
    progressValue = fraction < 0 ? -1 : fraction > 1 ? 1 : fraction;
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->open) apply_taskbar(windows[i]);
    return 1;
}

// Dragging files out: the shell makes the data object for the files, DoDragDrop runs the
// drag's modal loop from the pump, and the drop source follows the left button.
typedef struct DropSource {
    IDropSource source;
    LONG references;
} DropSource;

static HRESULT STDMETHODCALLTYPE source_query(IDropSource *self, REFIID riid, void **out) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropSource)) {
        *out = self;
        self->lpVtbl->AddRef(self);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE source_add_ref(IDropSource *self) {
    return (ULONG)InterlockedIncrement(&((DropSource *)self)->references);
}
static ULONG STDMETHODCALLTYPE source_release(IDropSource *self) {
    LONG left = InterlockedDecrement(&((DropSource *)self)->references);
    if (left == 0) free(self);
    return (ULONG)left;
}
static HRESULT STDMETHODCALLTYPE source_continue(IDropSource *self, BOOL escape, DWORD keys) {
    if (escape) return DRAGDROP_S_CANCEL;
    return (keys & MK_LBUTTON) ? S_OK : DRAGDROP_S_DROP;
}
static HRESULT STDMETHODCALLTYPE source_feedback(IDropSource *self, DWORD effect) {
    return DRAGDROP_S_USEDEFAULTCURSORS;
}
static IDropSourceVtbl dropSourceVtbl = {source_query, source_add_ref, source_release, source_continue,
                                         source_feedback};

static void run_file_drag(Task *task) {
    Win32Window *window = find(task->window);
    if (!window || !task->name) return;
    PIDLIST_ABSOLUTE items[64];
    UINT count = 0;
    for (WCHAR *line = task->name; *line && count < 64;) {
        WCHAR *end = wcschr(line, L'\n');
        if (end) *end = 0;
        if (*line) {
            PIDLIST_ABSOLUTE item = ILCreateFromPathW(line);
            if (item) items[count++] = item;
        }
        if (!end) break;
        line = end + 1;
    }
    IShellItemArray *array = NULL;
    IDataObject *data = NULL;
    if (count && SUCCEEDED(SHCreateShellItemArrayFromIDLists(count, (PCIDLIST_ABSOLUTE_ARRAY)items,
                                                             &array))) {
        array->lpVtbl->BindToHandler(array, NULL, &BHID_DataObject, &IID_IDataObject,
                                     (void **)&data);
        array->lpVtbl->Release(array);
    }
    for (UINT i = 0; i < count; i++) ILFree(items[i]);
    DropSource *source = data ? calloc(1, sizeof *source) : NULL;
    if (source) {
        source->source.lpVtbl = &dropSourceVtbl;
        source->references = 1;
        ReleaseCapture();
        DWORD effect = DROPEFFECT_NONE;
        DoDragDrop(data, &source->source, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
        source->source.lpVtbl->Release(&source->source);
    }
    if (data) data->lpVtbl->Release(data);
    // The drag loop took the button's release, so the window would never see it.
    POINT point;
    GetCursorPos(&point);
    ScreenToClient(window->hwnd, &point);
    SilkWindowEvent event = silk_event(SILK_EVENT_MOUSE_UP, window->id);
    event.x = point.x / window->scale;
    event.y = point.y / window->scale;
    event.modifiers = current_modifiers();
    silk_queue_push(&event);
}

uint8_t silk_window_start_file_drag(uint32_t id, const uint8_t *paths, size_t length) {
    if (!find(id) || !length || !(GetKeyState(VK_LBUTTON) & 0x8000)) return 0;
    Task *task = calloc(1, sizeof *task);
    if (!task) return 0;
    task->kind = 5;
    task->window = id;
    task->name = wide(paths, length);
    post_task(task);
    return 1;
}

uint8_t silk_window_open_url(const uint8_t *url, size_t length) {
    if (!length) return 0;
    WCHAR *target = wide(url, length);
    HINSTANCE result = ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
    free(target);
    return (INT_PTR)result > 32 ? 1 : 0;
}

uint8_t silk_window_reveal_path(const uint8_t *path, size_t length) {
    if (!length) return 0;
    WCHAR *target = wide(path, length);
    for (WCHAR *c = target; *c; c++)
        if (*c == L'/') *c = L'\\';
    PIDLIST_ABSOLUTE item = ILCreateFromPathW(target);
    free(target);
    if (!item) return 0;
    HRESULT shown = SHOpenFolderAndSelectItems(item, 0, NULL, 0);
    ILFree(item);
    return SUCCEEDED(shown) ? 1 : 0;
}

static void run_task(Task *task) {
    if (task->kind == 5) {
        run_file_drag(task);
    } else if (task->kind == 4) {
        run_context_menu(task);
    } else if (task->kind == 3) {
        Win32Window *window = find(task->window);
        if (window) {
            ReleaseCapture();
            SendMessageW(window->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
    } else {
        run_dialog(task);
    }
    free(task->title);
    free(task->directory);
    free(task->name);
    silk_free_extensions(task->extensions);
    free(task);
}

static uint32_t queue_dialog(int kind, const uint8_t *title, size_t titleLength,
                             const uint8_t *directory, size_t directoryLength, const uint8_t *name,
                             size_t nameLength, const uint8_t *extensions, size_t extensionsLength,
                             uint32_t flags, uint32_t parent) {
    Task *task = calloc(1, sizeof *task);
    if (!task) return 0;
    task->kind = kind;
    task->id = silk_next_dialog_id();
    task->window = parent;
    task->flags = flags;
    task->title = wide(title, titleLength);
    task->directory = (flags & 8) ? wide(directory, directoryLength) : NULL;
    task->name = wide(name, nameLength);
    size_t count = 0;
    task->extensions = silk_split_extensions(extensions, extensionsLength, &count);
    uint32_t id = task->id;
    post_task(task);
    return id;
}

uint32_t silk_window_open_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent) {
    return queue_dialog(1, title, titleLength, directory, directoryLength, NULL, 0, extensions,
                        extensionsLength, flags, parent);
}

uint32_t silk_window_save_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *name, size_t nameLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent) {
    return queue_dialog(2, title, titleLength, directory, directoryLength, name, nameLength,
                        extensions, extensionsLength, flags, parent);
}

// ---------------------------------------------------------------------------------------
// Clipboard.

// "HTML Format" and "PNG", registered by the apps that use them; CF_DIBV5 stands in for PNG
// for apps that only know bitmaps.
static UINT html_format(void) {
    static UINT format;
    if (!format) format = RegisterClipboardFormatW(L"HTML Format");
    return format;
}
static UINT png_format(void) {
    static UINT format;
    if (!format) format = RegisterClipboardFormatW(L"PNG");
    return format;
}

// WIC, for converting between PNG and bitmaps. The GUIDs are spelled out so that no import
// library is needed for them.
static const GUID wicFactoryClass = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID wicFactoryInterface = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID wicPng = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID wicBgra = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f}};

static IWICImagingFactory *wic(void) {
    static IWICImagingFactory *factory;
    if (!factory &&
        FAILED(CoCreateInstance(&wicFactoryClass, NULL, CLSCTX_INPROC_SERVER, &wicFactoryInterface,
                                (void **)&factory)))
        factory = NULL;
    return factory;
}

// `width` x `height` BGRA rows, top first, as PNG bytes (malloc'd), or NULL.
static uint8_t *encode_png(const uint8_t *bgra, UINT width, UINT height, size_t *length) {
    IWICImagingFactory *factory = wic();
    if (!factory) return NULL;
    uint8_t *out = NULL;
    IStream *stream = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream))) return NULL;
    WICPixelFormatGUID format = wicBgra;
    if (SUCCEEDED(IWICImagingFactory_CreateEncoder(factory, &wicPng, NULL, &encoder)) &&
        SUCCEEDED(IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL)) &&
        SUCCEEDED(IWICBitmapFrameEncode_Initialize(frame, NULL)) &&
        SUCCEEDED(IWICBitmapFrameEncode_SetSize(frame, width, height)) &&
        SUCCEEDED(IWICBitmapFrameEncode_SetPixelFormat(frame, &format)) && IsEqualGUID(&format, &wicBgra) &&
        SUCCEEDED(IWICBitmapFrameEncode_WritePixels(frame, height, width * 4, width * height * 4, (BYTE *)bgra)) &&
        SUCCEEDED(IWICBitmapFrameEncode_Commit(frame)) && SUCCEEDED(IWICBitmapEncoder_Commit(encoder))) {
        HGLOBAL memory = NULL;
        STATSTG stat;
        if (SUCCEEDED(GetHGlobalFromStream(stream, &memory)) &&
            SUCCEEDED(IStream_Stat(stream, &stat, STATFLAG_NONAME))) {
            *length = (size_t)stat.cbSize.QuadPart;
            out = malloc(*length ? *length : 1);
            if (!out) abort();
            memcpy(out, GlobalLock(memory), *length);
            GlobalUnlock(memory);
        }
    }
    if (frame) IWICBitmapFrameEncode_Release(frame);
    if (encoder) IWICBitmapEncoder_Release(encoder);
    IStream_Release(stream);
    return out;
}

// A packed DIB (CF_DIB or CF_DIBV5) of 24 or 32 bits as PNG bytes, or NULL.
static uint8_t *dib_to_png(const uint8_t *dib, size_t size, size_t *length) {
    if (size < sizeof(BITMAPINFOHEADER)) return NULL;
    const BITMAPINFOHEADER *header = (const BITMAPINFOHEADER *)dib;
    LONG width = header->biWidth, height = header->biHeight;
    bool bottomUp = height > 0;
    if (!bottomUp) height = -height;
    WORD bits = header->biBitCount;
    if (width <= 0 || height <= 0 || (bits != 24 && bits != 32)) return NULL;
    if (header->biCompression != BI_RGB && header->biCompression != BI_BITFIELDS) return NULL;
    size_t offset = header->biSize;
    if (header->biCompression == BI_BITFIELDS && header->biSize == sizeof(BITMAPINFOHEADER)) offset += 12;
    size_t stride = (((size_t)width * bits + 31) / 32) * 4;
    if (offset + stride * (size_t)height > size) return NULL;
    uint8_t *bgra = malloc((size_t)width * (size_t)height * 4);
    if (!bgra) abort();
    bool anyAlpha = false;
    for (LONG y = 0; y < height; y++) {
        const uint8_t *row = dib + offset + stride * (size_t)(bottomUp ? height - 1 - y : y);
        uint8_t *out = bgra + (size_t)y * (size_t)width * 4;
        for (LONG x = 0; x < width; x++) {
            const uint8_t *pixel = row + (size_t)x * (bits / 8);
            out[x * 4] = pixel[0];
            out[x * 4 + 1] = pixel[1];
            out[x * 4 + 2] = pixel[2];
            out[x * 4 + 3] = bits == 32 ? pixel[3] : 255;
            if (bits == 32 && pixel[3]) anyAlpha = true;
        }
    }
    // 32-bit bitmaps without alpha leave it 0: they are opaque.
    if (bits == 32 && !anyAlpha)
        for (size_t i = 3; i < (size_t)width * (size_t)height * 4; i += 4) bgra[i] = 255;
    uint8_t *png = encode_png(bgra, (UINT)width, (UINT)height, length);
    free(bgra);
    return png;
}

// PNG bytes as a CF_DIBV5 global (bottom-up BGRA), or NULL.
static HGLOBAL png_to_dib(const uint8_t *png, size_t length) {
    IWICImagingFactory *factory = wic();
    if (!factory) return NULL;
    HGLOBAL result = NULL;
    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    UINT width = 0, height = 0;
    if (SUCCEEDED(IWICImagingFactory_CreateStream(factory, &stream)) &&
        SUCCEEDED(IWICStream_InitializeFromMemory(stream, (BYTE *)png, (DWORD)length)) &&
        SUCCEEDED(IWICImagingFactory_CreateDecoderFromStream(factory, (IStream *)stream, NULL,
                                                             WICDecodeMetadataCacheOnDemand, &decoder)) &&
        SUCCEEDED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame)) &&
        SUCCEEDED(IWICImagingFactory_CreateFormatConverter(factory, &converter)) &&
        SUCCEEDED(IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame, &wicBgra,
                                                 WICBitmapDitherTypeNone, NULL, 0.0,
                                                 WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(IWICFormatConverter_GetSize(converter, &width, &height)) && width && height) {
        size_t stride = (size_t)width * 4, pixels = stride * height;
        uint8_t *rows = malloc(pixels);
        if (!rows) abort();
        if (SUCCEEDED(IWICFormatConverter_CopyPixels(converter, NULL, (UINT)stride, (UINT)pixels, rows))) {
            result = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPV5HEADER) + pixels);
            if (result) {
                uint8_t *out = GlobalLock(result);
                BITMAPV5HEADER *header = (BITMAPV5HEADER *)out;
                memset(header, 0, sizeof *header);
                header->bV5Size = sizeof *header;
                header->bV5Width = (LONG)width;
                header->bV5Height = (LONG)height;
                header->bV5Planes = 1;
                header->bV5BitCount = 32;
                header->bV5Compression = BI_BITFIELDS;
                header->bV5SizeImage = (DWORD)pixels;
                header->bV5RedMask = 0x00FF0000;
                header->bV5GreenMask = 0x0000FF00;
                header->bV5BlueMask = 0x000000FF;
                header->bV5AlphaMask = 0xFF000000;
                header->bV5CSType = 0x73524742; // LCS_sRGB, 'sRGB'
                header->bV5Intent = LCS_GM_IMAGES;
                for (UINT y = 0; y < height; y++)
                    memcpy(out + sizeof *header + stride * y, rows + stride * (height - 1 - y), stride);
                GlobalUnlock(result);
            }
        }
        free(rows);
    }
    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (stream) IWICStream_Release(stream);
    return result;
}

// The number after `key` in a CF_HTML header, or -1.
static long html_offset(const char *header, size_t length, const char *key) {
    size_t keyLength = strlen(key);
    for (size_t i = 0; i + keyLength < length && i < 1024; i++)
        if (memcmp(header + i, key, keyLength) == 0) return strtol(header + i + keyLength, NULL, 10);
    return -1;
}

// `html` wrapped in the CF_HTML header that gives the offsets of the document and fragment.
static HGLOBAL html_global(const uint8_t *html, size_t length) {
    static const char prefix[] = "<html><body>\r\n<!--StartFragment-->";
    static const char suffix[] = "<!--EndFragment-->\r\n</body></html>";
    char header[160];
    int headerLength = 0;
    for (int pass = 0; pass < 2; pass++) {
        size_t startFragment = (size_t)headerLength + sizeof prefix - 1;
        size_t endFragment = startFragment + length;
        headerLength = snprintf(header, sizeof header,
                                "Version:0.9\r\nStartHTML:%010d\r\nEndHTML:%010d\r\n"
                                "StartFragment:%010d\r\nEndFragment:%010d\r\n",
                                headerLength, (int)(endFragment + sizeof suffix - 1),
                                (int)startFragment, (int)endFragment);
    }
    size_t total = (size_t)headerLength + sizeof prefix - 1 + length + sizeof suffix - 1;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, total + 1);
    if (!memory) return NULL;
    char *out = GlobalLock(memory);
    memcpy(out, header, (size_t)headerLength);
    memcpy(out + headerLength, prefix, sizeof prefix - 1);
    memcpy(out + headerLength + sizeof prefix - 1, html, length);
    memcpy(out + headerLength + sizeof prefix - 1 + length, suffix, sizeof suffix);
    GlobalUnlock(memory);
    return memory;
}

int64_t silk_window_clipboard_types(uint8_t *destination, size_t capacity) {
    if (silk_read_needs_fetch(destination)) {
        bool has[SILK_CLIP_KINDS] = {
            IsClipboardFormatAvailable(CF_UNICODETEXT) != 0,
            IsClipboardFormatAvailable(html_format()) != 0,
            IsClipboardFormatAvailable(png_format()) || IsClipboardFormatAvailable(CF_DIBV5) ||
                IsClipboardFormatAvailable(CF_DIB),
        };
        char list[64];
        size_t length = silk_clip_types_of(has, list);
        silk_read_set(length ? (const uint8_t *)list : NULL, length);
    }
    return silk_read_serve(destination, capacity);
}

// Reads kind `kind` from the open clipboard into the read cache.
static void read_kind(int kind) {
    silk_read_set(NULL, 0);
    UINT format = kind == SILK_CLIP_TEXT ? CF_UNICODETEXT : kind == SILK_CLIP_HTML ? html_format() : png_format();
    HANDLE data = GetClipboardData(format);
    if (!data && kind == SILK_CLIP_PNG) {
        data = GetClipboardData(CF_DIBV5);
        if (!data) data = GetClipboardData(CF_DIB);
        if (!data) return;
        const uint8_t *dib = GlobalLock(data);
        if (!dib) return;
        size_t length = 0;
        uint8_t *png = dib_to_png(dib, GlobalSize(data), &length);
        GlobalUnlock(data);
        if (png) silk_read_adopt(png, length);
        return;
    }
    if (!data) return;
    const uint8_t *bytes = GlobalLock(data);
    if (!bytes) return;
    size_t size = GlobalSize(data);
    if (kind == SILK_CLIP_TEXT) {
        size_t length = 0;
        uint8_t *text = narrow((const WCHAR *)bytes, (int)wcsnlen((const WCHAR *)bytes, size / sizeof(WCHAR)), &length);
        silk_read_adopt(text, length);
    } else if (kind == SILK_CLIP_HTML) {
        // The fragment that was copied, as browsers read it; the whole document without one.
        long start = html_offset((const char *)bytes, size, "StartFragment:");
        long end = html_offset((const char *)bytes, size, "EndFragment:");
        if (start < 0 || end < start) {
            start = html_offset((const char *)bytes, size, "StartHTML:");
            end = html_offset((const char *)bytes, size, "EndHTML:");
        }
        if (start >= 0 && end >= start && (size_t)end <= size)
            silk_read_set(bytes + start, (size_t)(end - start));
    } else {
        silk_read_set(bytes, size);
    }
    GlobalUnlock(data);
}

int64_t silk_window_clipboard_read_type(const uint8_t *mime, size_t mimeLength,
                                        uint8_t *destination, size_t capacity) {
    int kind = silk_clip_kind(mime, mimeLength);
    if (kind < 0) return -1;
    if (silk_read_needs_fetch(destination)) {
        silk_read_set(NULL, 0);
        if (OpenClipboard(helper)) {
            read_kind(kind);
            CloseClipboard();
        }
    }
    return silk_read_serve(destination, capacity);
}

int64_t silk_window_clipboard_read(uint8_t *destination, size_t capacity) {
    return silk_window_clipboard_read_type((const uint8_t *)"text/plain", 10, destination, capacity);
}

void silk_window_clipboard_commit(void) {
    SilkClip clip = {0};
    silk_clip_take_pending(&clip);
    if (OpenClipboard(helper)) {
        EmptyClipboard();
        if (clip.has[SILK_CLIP_TEXT]) {
            WCHAR *converted = wide(clip.data[SILK_CLIP_TEXT].bytes, clip.data[SILK_CLIP_TEXT].length);
            size_t units = wcslen(converted) + 1;
            HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, units * sizeof(WCHAR));
            if (memory) {
                memcpy(GlobalLock(memory), converted, units * sizeof(WCHAR));
                GlobalUnlock(memory);
                if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
            }
            free(converted);
        }
        if (clip.has[SILK_CLIP_HTML]) {
            HGLOBAL memory = html_global(clip.data[SILK_CLIP_HTML].bytes, clip.data[SILK_CLIP_HTML].length);
            if (memory && !SetClipboardData(html_format(), memory)) GlobalFree(memory);
        }
        if (clip.has[SILK_CLIP_PNG]) {
            const SilkText *png = &clip.data[SILK_CLIP_PNG];
            HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, png->length ? png->length : 1);
            if (memory) {
                memcpy(GlobalLock(memory), png->bytes, png->length);
                GlobalUnlock(memory);
                if (!SetClipboardData(png_format(), memory)) GlobalFree(memory);
            }
            HGLOBAL dib = png_to_dib(png->bytes, png->length);
            if (dib && !SetClipboardData(CF_DIBV5, dib)) GlobalFree(dib);
        }
        CloseClipboard();
    }
    silk_clip_free(&clip);
}

void silk_window_clipboard_write(const uint8_t *text, size_t length) {
    silk_window_clipboard_begin();
    silk_window_clipboard_add((const uint8_t *)"text/plain", 10, text, length);
    silk_window_clipboard_commit();
}

// ---------------------------------------------------------------------------------------
// Test hooks: posted window messages, as the system would deliver input.

static UINT scan_of_code(uint32_t code, bool *extended) {
    for (UINT i = 0; i < 512; i++)
        if (silk_windows_key_codes[i] == code && code) {
            *extended = i >= 256;
            return i & 0xFF;
        }
    return 0;
}

uint8_t silk_window_test_key(uint32_t id, uint32_t code, const uint8_t *text, size_t length,
                             uint32_t modifiers) {
    (void)text;
    (void)length;
    Win32Window *window = find(id);
    bool extended = false;
    UINT scan = scan_of_code(code, &extended);
    if (!window || !scan) return 0;
    UINT vk = MapVirtualKeyW(scan | (extended ? 0xE000 : 0), MAPVK_VSC_TO_VK_EX);
    if (!vk) return 0;
    // The keyboard state the key is read with, restored after the release.
    BYTE *saved = malloc(256);
    BYTE state[256];
    if (!saved) return 0;
    GetKeyboardState(saved);
    memcpy(state, saved, 256);
    state[VK_SHIFT] = state[VK_LSHIFT] = (modifiers & SILK_MOD_SHIFT) ? 0x80 : 0;
    state[VK_CONTROL] = state[VK_LCONTROL] = (modifiers & SILK_MOD_CONTROL) ? 0x80 : 0;
    state[VK_MENU] = state[VK_LMENU] = (modifiers & SILK_MOD_ALT) ? 0x80 : 0;
    state[VK_LWIN] = (modifiers & SILK_MOD_META) ? 0x80 : 0;
    state[VK_RMENU] = 0;
    SetKeyboardState(state);
    LPARAM lParam = 1 | ((LPARAM)scan << 16) | (extended ? (1 << 24) : 0);
    bool system = (modifiers & SILK_MOD_ALT) != 0;
    PostMessageW(window->hwnd, system ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, lParam);
    PostMessageW(window->hwnd, system ? WM_SYSKEYUP : WM_KEYUP, vk,
                 lParam | (1 << 30) | ((LPARAM)1 << 31));
    PostMessageW(helper, WM_SILK_KEYSTATE, 0, (LPARAM)saved);
    return 1;
}

uint8_t silk_window_test_mouse(uint32_t id, uint32_t kind, uint32_t button, double x, double y,
                               double dx, double dy) {
    Win32Window *window = find(id);
    if (!window) return 0;
    int px = (int)lround(x * window->scale), py = (int)lround(y * window->scale);
    LPARAM position = MAKELPARAM(px, py);
    // Put the real cursor there too: Windows synthesizes moves at the cursor's position
    // (after tracking starts, after clicks), which would otherwise report somewhere else.
    POINT cursor = {px, py};
    ClientToScreen(window->hwnd, &cursor);
    SetCursorPos(cursor.x, cursor.y);
    if (kind == 0) {
        PostMessageW(window->hwnd, WM_MOUSEMOVE, 0, position);
    } else if (kind == 1 || kind == 2) {
        static const UINT down[] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN, WM_XBUTTONDOWN, WM_XBUTTONDOWN};
        static const UINT up[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP, WM_XBUTTONUP, WM_XBUTTONUP};
        if (button > 4) button = 0;
        WPARAM wParam = button == 3 ? MAKEWPARAM(0, XBUTTON1) : button == 4 ? MAKEWPARAM(0, XBUTTON2) : 0;
        PostMessageW(window->hwnd, kind == 1 ? down[button] : up[button], wParam, position);
    } else {
        POINT screen = {px, py};
        ClientToScreen(window->hwnd, &screen);
        LPARAM at = MAKELPARAM(screen.x, screen.y);
        if (dy != 0) PostMessageW(window->hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, (short)lround(dy * WHEEL_DELTA)), at);
        if (dx != 0) PostMessageW(window->hwnd, WM_MOUSEHWHEEL, MAKEWPARAM(0, (short)lround(-dx * WHEEL_DELTA)), at);
    }
    return 1;
}

uint8_t silk_window_test_preedit(uint32_t id, const uint8_t *text, size_t length, uint64_t start,
                                 uint64_t end) {
    (void)end;
    Win32Window *window = find(id);
    if (!window) return 0;
    WCHAR *converted = wide(text, length);
    // The caret as the input method reports it, in UTF-16 units.
    int caret = start >= length ? (int)wcslen(converted)
                                : MultiByteToWideChar(CP_UTF8, 0, (const char *)text, (int)start, NULL, 0);
    push_preedit(window, converted, (int)wcslen(converted), caret);
    free(converted);
    return 1;
}

uint8_t silk_window_test_commit(uint32_t id, const uint8_t *text, size_t length) {
    Win32Window *window = find(id);
    if (!window) return 0;
    WCHAR *converted = wide(text, length);
    push_preedit(window, L"", 0, 0);
    push_commit(window, converted, (int)wcslen(converted));
    free(converted);
    return 1;
}

uint8_t silk_window_test_resize(uint32_t id, double width, double height) {
    Win32Window *window = find(id);
    if (!window) return 0;
    silk_window_set_content_size(id, width, height);
    return 1;
}

void silk_window_test_ime_rect(uint32_t id, double *x, double *y, double *width, double *height) {
    *x = *y = *width = *height = 0;
    Win32Window *window = find(id);
    if (!window) return;
    *x = window->imeX;
    *y = window->imeY;
    *width = window->imeWidth;
    *height = window->imeHeight;
}

// ---------------------------------------------------------------------------------------
// The tray: an icon in the notification area, owned by a hidden top-level window (a
// context menu only closes on an outside click when its owner can be the foreground window).
// Notifications are the icon's balloons, which Windows 10 and later show as toasts.

static bool trayShown;
static HICON trayIcon;
static HMENU trayMenu;
static WCHAR trayTip[128];
static bool noticeIcon; // NOTICE_ICON was added for a notification
static uint32_t noticeId;
static uint32_t nextNotice = 1;
static UINT taskbarCreated;

static void ensure_tray_window(void) {
    if (trayWindow || !started) return;
    trayWindow = CreateWindowExW(WS_EX_TOOLWINDOW, L"SilkWindow", L"", WS_POPUP, 0, 0, 0, 0, NULL,
                                 NULL, instance, NULL);
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
}

static HICON icon_from_rgba(const uint8_t *pixels, uint32_t width, uint32_t height) {
    if (!width || !height) return NULL;
    BITMAPV5HEADER header;
    memset(&header, 0, sizeof header);
    header.bV5Size = sizeof header;
    header.bV5Width = (LONG)width;
    header.bV5Height = -(LONG)height;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000;
    header.bV5GreenMask = 0x0000FF00;
    header.bV5BlueMask = 0x000000FF;
    header.bV5AlphaMask = 0xFF000000;
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP color = CreateDIBSection(screen, (BITMAPINFO *)&header, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!color) return NULL;
    uint8_t *out = bits;
    for (size_t i = 0; i < (size_t)width * height; i++) {
        out[i * 4 + 0] = pixels[i * 4 + 2];
        out[i * 4 + 1] = pixels[i * 4 + 1];
        out[i * 4 + 2] = pixels[i * 4 + 0];
        out[i * 4 + 3] = pixels[i * 4 + 3];
    }
    HBITMAP mask = CreateBitmap((int)width, (int)height, 1, 1, NULL);
    ICONINFO info = {TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

static void tray_data(NOTIFYICONDATAW *data, UINT id) {
    memset(data, 0, sizeof *data);
    data->cbSize = sizeof *data;
    data->hWnd = trayWindow;
    data->uID = id;
    data->uCallbackMessage = WM_SILK_TRAY;
    data->uVersion = NOTIFYICON_VERSION_4;
}

static bool tray_add(void) {
    NOTIFYICONDATAW data;
    tray_data(&data, TRAY_ICON);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.hIcon = trayIcon;
    wcsncpy(data.szTip, trayTip, 127);
    if (!Shell_NotifyIconW(NIM_ADD, &data) && !Shell_NotifyIconW(NIM_MODIFY, &data)) return false;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

uint8_t silk_window_tray_commit(const uint8_t *pixels, uint32_t width, uint32_t height,
                                const uint8_t *tooltip, size_t tooltipLength, uint32_t flags) {
    (void)flags;
    silk_tray_commit_pending();
    ensure_tray_window();
    if (!trayWindow) return 0;
    HICON icon = icon_from_rgba(pixels, width, height);
    if (!icon) return 0;
    if (trayIcon) DestroyIcon(trayIcon);
    trayIcon = icon;
    if (trayMenu) DestroyMenu(trayMenu);
    trayMenu = build_menu_from(silk_tray_entries, silk_tray_count, true);
    WCHAR *tip = wide(tooltip, tooltipLength);
    trayTip[0] = 0;
    if (tip) wcsncpy(trayTip, tip, 127);
    trayTip[127] = 0;
    free(tip);
    trayShown = tray_add();
    return trayShown ? 1 : 0;
}

void silk_window_tray_remove(void) {
    if (!trayShown) return;
    NOTIFYICONDATAW data;
    tray_data(&data, TRAY_ICON);
    Shell_NotifyIconW(NIM_DELETE, &data);
    trayShown = false;
}

static void remove_notice_icon(void) {
    if (!noticeIcon) return;
    NOTIFYICONDATAW data;
    tray_data(&data, NOTICE_ICON);
    Shell_NotifyIconW(NIM_DELETE, &data);
    noticeIcon = false;
}

uint32_t silk_window_notify(const uint8_t *title, size_t titleLength, const uint8_t *body,
                            size_t bodyLength) {
    ensure_tray_window();
    if (!trayWindow) return 0;
    NOTIFYICONDATAW data;
    UINT id = trayShown ? TRAY_ICON : NOTICE_ICON;
    tray_data(&data, id);
    if (!trayShown && !noticeIcon) {
        data.uFlags = NIF_MESSAGE | NIF_ICON;
        data.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
        if (!Shell_NotifyIconW(NIM_ADD, &data)) return 0;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
        noticeIcon = true;
    }
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    WCHAR *wideTitle = wide(title, titleLength), *wideBody = wide(body, bodyLength);
    if (wideTitle) wcsncpy(data.szInfoTitle, wideTitle, 63);
    if (wideBody) wcsncpy(data.szInfo, wideBody, 255);
    free(wideTitle);
    free(wideBody);
    if (!data.szInfo[0]) wcsncpy(data.szInfo, L" ", 255);
    if (!Shell_NotifyIconW(NIM_MODIFY, &data)) return 0;
    noticeId = nextNotice++;
    return noticeId;
}

static void tray_menu_or_click(uint32_t button, int x, int y) {
    if (!trayMenu) {
        SilkWindowEvent event = silk_event(SILK_EVENT_TRAY_CLICKED, 0);
        event.code = button;
        silk_queue_push(&event);
        return;
    }
    SetForegroundWindow(trayWindow);
    UINT command = (UINT)TrackPopupMenu(trayMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                        x, y, 0, trayWindow, NULL);
    PostMessageW(trayWindow, WM_NULL, 0, 0);
    if (command >= MENU_ACTION_BASE) {
        SilkWindowEvent event = silk_event(SILK_EVENT_MENU_ACTION, 0);
        event.code = command - MENU_ACTION_BASE;
        silk_queue_push(&event);
    } else if (command >= MENU_SYSTEM_BASE) {
        system_item(windowCount ? windows[0] : NULL, command - MENU_SYSTEM_BASE);
    }
}

static LRESULT tray_message(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == taskbarCreated && taskbarCreated) {
        // Explorer restarted and forgot the icons.
        if (trayShown) tray_add();
        noticeIcon = false;
        return 0;
    }
    if (message != WM_SILK_TRAY) return DefWindowProcW(trayWindow, message, wParam, lParam);
    UINT event = LOWORD(lParam), icon = HIWORD(lParam);
    int x = (short)LOWORD(wParam), y = (short)HIWORD(wParam);
    switch (event) {
    case NIN_BALLOONUSERCLICK: {
        SilkWindowEvent clicked = silk_event(SILK_EVENT_NOTIFICATION_CLICKED, 0);
        clicked.code = noticeId;
        silk_queue_push(&clicked);
        if (icon == NOTICE_ICON) remove_notice_icon();
        break;
    }
    case NIN_BALLOONTIMEOUT:
    case NIN_BALLOONHIDE:
        if (icon == NOTICE_ICON) remove_notice_icon();
        break;
    case NIN_SELECT:
    case NIN_KEYSELECT:
        if (icon == TRAY_ICON) tray_menu_or_click(SILK_BUTTON_LEFT, x, y);
        break;
    case WM_CONTEXTMENU:
        if (icon == TRAY_ICON) tray_menu_or_click(SILK_BUTTON_RIGHT, x, y);
        break;
    case WM_MBUTTONUP:
        if (icon == TRAY_ICON && !trayMenu) tray_menu_or_click(SILK_BUTTON_MIDDLE, x, y);
        break;
    default:
        break;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------
// Global shortcuts: RegisterHotKey on the helper window. Hotkey ids are slots + 1.

typedef struct Hotkey {
    bool used;
    uint32_t id;
} Hotkey;

static Hotkey hotkeys[64];

void silk_window_shortcut_unregister(uint32_t id) {
    for (int i = 0; i < 64; i++) {
        if (!hotkeys[i].used || hotkeys[i].id != id) continue;
        UnregisterHotKey(helper, i + 1);
        hotkeys[i].used = false;
    }
}

uint8_t silk_window_shortcut_register(uint32_t id, uint32_t code, uint32_t modifiers) {
    if (!started) return 0;
    silk_window_shortcut_unregister(id);
    bool extended = false;
    UINT scan = scan_of_code(code, &extended);
    UINT vk = scan ? MapVirtualKeyW(scan | (extended ? 0xE000 : 0), MAPVK_VSC_TO_VK_EX) : 0;
    if (!vk) return 0;
    UINT mods = MOD_NOREPEAT;
    if (modifiers & SILK_MOD_SHIFT) mods |= MOD_SHIFT;
    if (modifiers & SILK_MOD_CONTROL) mods |= MOD_CONTROL;
    if (modifiers & SILK_MOD_ALT) mods |= MOD_ALT;
    if (modifiers & SILK_MOD_META) mods |= MOD_WIN;
    for (int i = 0; i < 64; i++) {
        if (hotkeys[i].used) continue;
        if (!RegisterHotKey(helper, i + 1, mods, vk)) return 0;
        hotkeys[i].used = true;
        hotkeys[i].id = id;
        return 1;
    }
    return 0;
}

static void hotkey_pressed(int hotkey) {
    if (hotkey < 1 || hotkey > 64 || !hotkeys[hotkey - 1].used) return;
    SilkWindowEvent event = silk_event(SILK_EVENT_GLOBAL_SHORTCUT, 0);
    event.code = hotkeys[hotkey - 1].id;
    silk_queue_push(&event);
}

uint8_t silk_window_test_drop(uint32_t window, uint32_t kind, double x, double y,
                              const uint8_t *paths, size_t length) {
    // OLE drags come from another process's DoDragDrop; there is nothing to post.
    return 0;
}
