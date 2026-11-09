// The C ABI between native/platform/window's Silk code (src/bridge.silk) and the Windows, Linux and
// browser backends. The macOS backend has its own ABI (native/macos/bridge.h) with the same
// event record.
//
// Every function must be called on the thread that called silk_window_start. Strings are
// UTF-8 with a length and no terminator. Nothing here calls back into Silk. The browser
// backend implements the same functions in JavaScript (browser/window-host.js), imported by
// the wasm module under the "window" module name.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Event kinds; src/bridge_sys.silk decodes the same numbers. They match the macOS bridge.
enum {
    SILK_EVENT_REQUEST_FRAME = 1,
    SILK_EVENT_RESIZED = 2,
    SILK_EVENT_SCALE_FACTOR_CHANGED = 3,
    SILK_EVENT_MOVED = 4,
    SILK_EVENT_FOCUSED = 5,
    SILK_EVENT_OCCLUDED = 6,
    SILK_EVENT_CLOSE_REQUESTED = 7,
    SILK_EVENT_FULL_SCREEN_CHANGED = 8,
    SILK_EVENT_KEY_DOWN = 9,
    SILK_EVENT_KEY_UP = 10,
    SILK_EVENT_MODIFIERS_CHANGED = 11,
    SILK_EVENT_TEXT_INPUT = 12,
    SILK_EVENT_IME_PREEDIT = 13,
    SILK_EVENT_MOUSE_MOVED = 14,
    SILK_EVENT_MOUSE_DOWN = 15,
    SILK_EVENT_MOUSE_UP = 16,
    SILK_EVENT_MOUSE_ENTERED = 17,
    SILK_EVENT_MOUSE_EXITED = 18,
    SILK_EVENT_SCROLL = 19,
    SILK_EVENT_PINCH = 20,
    SILK_EVENT_MENU_ACTION = 21,
    SILK_EVENT_DIALOG_CLOSED = 22,
    SILK_EVENT_APPEARANCE_CHANGED = 23,
    SILK_EVENT_QUIT_REQUESTED = 24,
};

// flags bits.
enum {
    SILK_FLAG_REPEAT = 1,       // KeyDown
    SILK_FLAG_IME_HANDLED = 2,  // KeyDown
    SILK_FLAG_ON = 4,           // Focused, Occluded, FullScreenChanged
    SILK_FLAG_PRECISE = 8,      // Scroll in points rather than lines
};

// Modifier bits, as event.Modifiers.bits().
enum {
    SILK_MOD_SHIFT = 1,
    SILK_MOD_CONTROL = 2,
    SILK_MOD_ALT = 4,
    SILK_MOD_META = 8,
    SILK_MOD_CAPS_LOCK = 16,
    SILK_MOD_FUNCTION = 32,
};

// Window option flags for silk_window_open.
enum {
    SILK_WINDOW_RESIZABLE = 1,
    SILK_WINDOW_VISIBLE = 2,
    SILK_WINDOW_TRANSPARENT = 4,
    SILK_WINDOW_TITLE_TRANSPARENT = 8,
    SILK_WINDOW_TITLE_HIDDEN = 16,
};

// silk_window_backend results.
enum {
    SILK_BACKEND_NONE = 0,
    SILK_BACKEND_WIN32 = 1,
    SILK_BACKEND_WAYLAND = 2,
    SILK_BACKEND_X11 = 3,
    SILK_BACKEND_BROWSER = 4,
};

// silk_window_raw_surface results, as webgpu.surface.SurfaceTarget kinds.
enum {
    SILK_SURFACE_NONE = 0,
    SILK_SURFACE_WAYLAND = 2, // first: wl_display, second: wl_surface
    SILK_SURFACE_XLIB = 3,    // first: Display, id: Window
    SILK_SURFACE_WIN32 = 4,   // first: HWND, second: HINSTANCE
    SILK_SURFACE_CANVAS = 5,  // id: the canvas handle the host page registered
};

// Mouse buttons, as event.MouseButton.
enum {
    SILK_BUTTON_LEFT = 0,
    SILK_BUTTON_RIGHT = 1,
    SILK_BUTTON_MIDDLE = 2,
    SILK_BUTTON_BACK = 3,
    SILK_BUTTON_FORWARD = 4,
    SILK_BUTTON_OTHER = 5,
};

// One event. Which fields carry what depends on `kind`; see src/bridge.silk.
typedef struct SilkWindowEvent {
    uint32_t kind;
    uint32_t window;
    uint32_t code;       // KeyCode, MouseButton, Appearance, menu action, dialog id
    uint32_t named;      // NamedKey
    uint32_t modifiers;  // event.Modifiers bits
    uint32_t flags;
    uint32_t count;      // click count, number of strings
    uint32_t phase;      // Phase for scroll and pinch
    double x, y;         // position or size in points
    double dx, dy;       // scroll delta, pinch delta in dx
    double value;        // scale factor, target time
    uint64_t start, end; // preedit selection in UTF-8 bytes
} SilkWindowEvent;

// 0 started, 1 already started, 3 no display server or not supported here.
int32_t silk_window_start(void);
uint32_t silk_window_backend(void);
// Returns the new window's id, or 0 when the system refused.
uint32_t silk_window_open(const uint8_t *title, size_t titleLength, double width, double height,
                          double minWidth, double minHeight, uint32_t flags);
void silk_window_close(uint32_t window);
// timeout < 0 waits forever, 0 polls. Returns 1 and fills `event` when one is ready.
uint8_t silk_window_next_event(double timeout, SilkWindowEvent *event);
// Copies up to `capacity` bytes of the `index`th string of the event last returned and
// returns its whole length. Valid until the next silk_window_next_event.
size_t silk_window_event_text(size_t index, uint8_t *destination, size_t capacity);
// From any thread: makes the current or next silk_window_next_event return soon, with or
// without an event. Does nothing before silk_window_start.
void silk_window_wake(void);

void silk_window_request_frame(uint32_t window);
void silk_window_content_size(uint32_t window, double *width, double *height);
double silk_window_scale_factor(uint32_t window);
// Fills the native handles and returns a SILK_SURFACE_* kind.
uint32_t silk_window_raw_surface(uint32_t window, void **first, void **second, uint64_t *id);
void silk_window_set_title(uint32_t window, const uint8_t *title, size_t length);
void silk_window_set_content_size(uint32_t window, double width, double height);
void silk_window_set_min_content_size(uint32_t window, double width, double height);
void silk_window_set_visible(uint32_t window, uint8_t visible);
void silk_window_focus(uint32_t window);
void silk_window_minimize(uint32_t window);
void silk_window_toggle_full_screen(uint32_t window);
void silk_window_start_window_drag(uint32_t window);
void silk_window_set_cursor(uint32_t window, uint32_t cursor);
void silk_window_set_ime(uint32_t window, uint8_t enabled, double x, double y, double width,
                         double height);

// Menu bars are described in order between begin_bar and commit, as on macOS. Backends
// without native menus keep the shortcuts and deliver MenuAction when one is pressed.
void silk_window_menu_begin_bar(void);
void silk_window_menu_begin(const uint8_t *title, size_t length);
void silk_window_menu_end(void);
// keyKind 0 none, 1 character (keyValue is a Unicode scalar), 2 NamedKey.
void silk_window_menu_action(const uint8_t *title, size_t length, uint32_t action,
                             uint32_t keyKind, uint32_t keyValue, uint32_t modifiers,
                             uint8_t enabled, uint8_t checked);
void silk_window_menu_separator(void);
void silk_window_menu_system(uint32_t item);
void silk_window_menu_commit(void);

// Both return a dialog id, or 0 when no dialog could be shown.
// flags: 1 files, 2 directories, 4 multiple, 8 has directory. parent 0 means none.
uint32_t silk_window_open_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent);
uint32_t silk_window_save_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *name, size_t nameLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent);

// Copies up to `capacity` bytes of the clipboard's text and returns its whole length, or -1
// when the clipboard holds no text. The text stays the same until the next write or until
// silk_window_next_event runs, so a second call with a large enough buffer reads it all.
int64_t silk_window_clipboard_read(uint8_t *destination, size_t capacity);
void silk_window_clipboard_write(const uint8_t *text, size_t length);
// 0 light, 1 dark.
uint32_t silk_window_appearance(void);

// Test hooks for the smoke tests: drive the system as the user would. Each returns 1 when
// it did something and 0 when the backend cannot simulate it.
//
// A key press and release of `code` (a KeyCode discriminant), typing `text`.
uint8_t silk_window_test_key(uint32_t window, uint32_t code, const uint8_t *text, size_t length,
                             uint32_t modifiers);
// kind 0 move, 1 press, 2 release, 3 wheel (dx, dy in lines).
uint8_t silk_window_test_mouse(uint32_t window, uint32_t kind, uint32_t button, double x,
                               double y, double dx, double dy);
// Composition text from an input method, selection in UTF-8 bytes; then a commit.
uint8_t silk_window_test_preedit(uint32_t window, const uint8_t *text, size_t length,
                                 uint64_t start, uint64_t end);
uint8_t silk_window_test_commit(uint32_t window, const uint8_t *text, size_t length);
// Resizes the window as the window manager would.
uint8_t silk_window_test_resize(uint32_t window, double width, double height);
// Where the backend told the input method the caret is, in points of the content area.
void silk_window_test_ime_rect(uint32_t window, double *x, double *y, double *width,
                               double *height);
