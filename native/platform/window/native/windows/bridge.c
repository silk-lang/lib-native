// The Win32 backend of native/platform/window: native/bridge.h over USER32, IMM32 and DWM.
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
#include <shlobj.h>
#include <shobjidl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "../common/core.h"
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
    bool imeEnabled;
    HIMC savedContext;
    double imeX, imeY, imeWidth, imeHeight;
    WCHAR highSurrogate;
    double lastClickTime;
    int lastClickX, lastClickY;
    uint32_t lastClickButton, clickCount;
} Win32Window;

typedef struct Task {
    int kind; // 1 open dialog, 2 save dialog, 3 window drag
    uint32_t id;
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

// Clipboard.
static uint8_t *clipboardText;
static int64_t clipboardLength = -1;

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

static HMENU build_menu(void) {
    if (!silk_menu_count) return NULL;
    HMENU bar = CreateMenu();
    HMENU stack[32];
    int depth = 0;
    stack[0] = bar;
    for (size_t i = 0; i < silk_menu_count; i++) {
        const SilkMenuEntry *entry = &silk_menu_entries[i];
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

static HCURSOR cursor_handle(uint32_t cursor) {
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
    default:
        break;
    }
}

static void run_task(Task *task);

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
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
            SetCursor(cursor_handle(window->cursor));
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
    if (flags & SILK_WINDOW_VISIBLE) silk_window_set_visible(window->id, 1);
    return window->id;
}

void silk_window_close(uint32_t id) {
    Win32Window *window = find(id);
    if (!window) return;
    window->open = false;
    if (window->savedContext) ImmAssociateContext(window->hwnd, window->savedContext);
    DestroyWindow(window->hwnd);
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

void silk_window_set_cursor(uint32_t id, uint32_t cursor) {
    Win32Window *window = find(id);
    if (!window) return;
    window->cursor = cursor;
    POINT point;
    GetCursorPos(&point);
    if (WindowFromPoint(point) == window->hwnd) {
        ScreenToClient(window->hwnd, &point);
        RECT client;
        GetClientRect(window->hwnd, &client);
        if (PtInRect(&client, point)) SetCursor(cursor_handle(cursor));
    }
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

static void run_task(Task *task) {
    if (task->kind == 3) {
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

int64_t silk_window_clipboard_read(uint8_t *destination, size_t capacity) {
    if (!destination || clipboardLength < 0) {
        free(clipboardText);
        clipboardText = NULL;
        clipboardLength = -1;
        if (!OpenClipboard(helper)) return -1;
        HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (data) {
            const WCHAR *text = GlobalLock(data);
            if (text) {
                size_t length = 0;
                clipboardText = narrow(text, -1, &length);
                clipboardLength = (int64_t)length;
                GlobalUnlock(data);
            }
        }
        CloseClipboard();
        if (clipboardLength < 0) return -1;
    }
    if (destination && capacity)
        memcpy(destination, clipboardText, (size_t)clipboardLength < capacity ? (size_t)clipboardLength : capacity);
    int64_t length = clipboardLength;
    if (destination) clipboardLength = -1; // the next read asks the system again
    return length;
}

void silk_window_clipboard_write(const uint8_t *text, size_t length) {
    WCHAR *converted = wide(text, length);
    size_t units = wcslen(converted) + 1;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, units * sizeof(WCHAR));
    if (memory) {
        memcpy(GlobalLock(memory), converted, units * sizeof(WCHAR));
        GlobalUnlock(memory);
        if (OpenClipboard(helper)) {
            EmptyClipboard();
            if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
            CloseClipboard();
        } else {
            GlobalFree(memory);
        }
    }
    free(converted);
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
