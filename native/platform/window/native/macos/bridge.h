// The C ABI between native/platform/window's Silk code (src/macos.silk) and the AppKit backend.
// Every function must be called on the main thread. Strings are UTF-8 with a length and
// no terminator. Nothing here calls back into Silk.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Event kinds; src/macos.silk decodes the same numbers.
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

// Window option flags for silk_macos_open_window.
enum {
    SILK_WINDOW_RESIZABLE = 1,
    SILK_WINDOW_VISIBLE = 2,
    SILK_WINDOW_TRANSPARENT = 4,
    SILK_WINDOW_TITLE_TRANSPARENT = 8,
    SILK_WINDOW_TITLE_HIDDEN = 16,
};

// One event. Which fields carry what depends on `kind`; see src/macos.silk.
typedef struct SilkWindowEvent {
    uint32_t kind;
    uint32_t window;
    uint32_t code;       // KeyCode, MouseButton, Phase, Appearance, menu action, dialog id
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

// 0 started, 1 already started, 2 not on the main thread, 3 no window server.
int32_t silk_macos_start(void);
uint32_t silk_macos_open_window(const uint8_t *title, size_t titleLength, double width,
                                double height, double minWidth, double minHeight,
                                uint32_t flags);
void silk_macos_close_window(uint32_t window);
// timeout < 0 waits forever, 0 polls. Returns 1 and fills `event` when one is ready.
uint8_t silk_macos_next_event(double timeout, SilkWindowEvent *event);
// The `index`th string of the event last returned, valid until the next call to
// silk_macos_next_event. NULL past the last one.
const uint8_t *silk_macos_event_text(size_t index, size_t *length);
// From any thread: makes the current or next silk_macos_next_event return soon, with or
// without an event.
void silk_macos_wake(void);

void silk_macos_request_frame(uint32_t window);
void silk_macos_content_size(uint32_t window, double *width, double *height);
double silk_macos_scale_factor(uint32_t window);
void *silk_macos_metal_layer(uint32_t window);
void silk_macos_set_title(uint32_t window, const uint8_t *title, size_t length);
void silk_macos_set_content_size(uint32_t window, double width, double height);
void silk_macos_set_min_content_size(uint32_t window, double width, double height);
void silk_macos_set_visible(uint32_t window, uint8_t visible);
void silk_macos_focus(uint32_t window);
void silk_macos_minimize(uint32_t window);
void silk_macos_toggle_full_screen(uint32_t window);
void silk_macos_start_window_drag(uint32_t window);
void silk_macos_set_cursor(uint32_t window, uint32_t cursor);
void silk_macos_set_ime(uint32_t window, uint8_t enabled, double x, double y, double width,
                        double height);

// Menu bars are described in order between begin_bar and commit.
void silk_macos_menu_begin_bar(void);
void silk_macos_menu_begin(const uint8_t *title, size_t length);
void silk_macos_menu_end(void);
// keyKind 0 none, 1 character (keyValue is a Unicode scalar), 2 NamedKey.
void silk_macos_menu_action(const uint8_t *title, size_t length, uint32_t action,
                            uint32_t keyKind, uint32_t keyValue, uint32_t modifiers,
                            uint8_t enabled, uint8_t checked);
void silk_macos_menu_separator(void);
void silk_macos_menu_system(uint32_t item);
void silk_macos_menu_commit(void);

// Both return a dialog id, or 0 when the panel could not be created.
// flags: 1 files, 2 directories, 4 multiple, 8 has directory. parent 0 means none.
uint32_t silk_macos_open_dialog(const uint8_t *title, size_t titleLength,
                                const uint8_t *directory, size_t directoryLength,
                                const uint8_t *extensions, size_t extensionsLength,
                                uint32_t flags, uint32_t parent);
uint32_t silk_macos_save_dialog(const uint8_t *title, size_t titleLength,
                                const uint8_t *directory, size_t directoryLength,
                                const uint8_t *name, size_t nameLength,
                                const uint8_t *extensions, size_t extensionsLength,
                                uint32_t flags, uint32_t parent);

// The clipboard's text, valid until the next read; NULL when it holds none.
const uint8_t *silk_macos_clipboard_read(size_t *length);
void silk_macos_clipboard_write(const uint8_t *text, size_t length);
// 0 light, 1 dark.
uint32_t silk_macos_appearance(void);

// Test hooks for the smoke test: drive AppKit as the user would.
void silk_macos_test_post_key(uint32_t window, uint16_t keyCode, const uint8_t *characters,
                              size_t length, uint32_t modifiers);
void silk_macos_test_set_marked_text(uint32_t window, const uint8_t *text, size_t length,
                                     uint64_t selectionStart, uint64_t selectionLength);
void silk_macos_test_insert_text(uint32_t window, const uint8_t *text, size_t length);
void silk_macos_test_set_frame_size(uint32_t window, double width, double height);
// The screen rectangle the input method would place its candidate window at.
void silk_macos_test_ime_rect(uint32_t window, double *x, double *y, double *width,
                              double *height);
uint8_t silk_macos_test_uses_fiber(void);
