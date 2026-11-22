// The C ABI between modules/platform/window's Silk code (src/macos.silk) and the AppKit backend.
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
    SILK_EVENT_FILE_DRAG_ENTERED = 25, // x, y; count strings: the paths, when known
    SILK_EVENT_FILE_DRAG_MOVED = 26,   // x, y
    SILK_EVENT_FILE_DRAG_LEFT = 27,
    SILK_EVENT_FILE_DROPPED = 28,      // x, y; count strings: the paths
    SILK_EVENT_GLOBAL_SHORTCUT = 29,   // code: the shortcut's id
    SILK_EVENT_TRAY_CLICKED = 30,      // code: the mouse button
    SILK_EVENT_NOTIFICATION_CLICKED = 31, // code: the notification's id
    SILK_EVENT_CONTEXT_MENU_CLOSED = 32,  // a context menu closed with nothing chosen
    SILK_EVENT_TOUCH = 33,                // never sent: Macs have no touchscreens
    SILK_EVENT_EDIT_COMMAND = 34,         // window; code: menu.EditCommand (the Edit menu)
};

// tray_commit flags.
enum {
    SILK_TRAY_TEMPLATE = 1, // macOS: draw the icon from its alpha in the menu bar's colours
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

/// A GPU canvas layer's place and look (bridge.m, "GPU canvas layers"). Rects are device pixels
/// from the window's top-left corner.
typedef struct SilkCanvasLayerState {
    double x, y, width, height;
    double clip_x, clip_y, clip_width, clip_height;
    double corner_radius;
    double opacity;
    double scale;
    uint32_t opaque;
    uint32_t hidden;
    uint32_t masks;
} SilkCanvasLayerState;

void silk_ca_begin(void);
void silk_ca_commit(void);
void *silk_ca_canvas_layer_create(void *parent);
void silk_ca_canvas_layer_destroy(void *handle);
void silk_ca_canvas_layer_set(void *handle, const SilkCanvasLayerState *state, void *io_surface);
void silk_ca_canvas_layer_get(void *handle, SilkCanvasLayerState *out, void **io_surface);
int silk_ca_layer_index_of(void *parent, void *handle);
void silk_ca_metal_layer_presents_with_transaction(void *layer, int on);
int silk_ca_metal_layer_presents_with_transaction_get(void *layer);
void *silk_ca_detached_layer_create(double width, double height, int flipped, int metal);
void silk_ca_layer_release(void *layer);
void *silk_ca_test_iosurface_create(uint32_t width, uint32_t height);
void silk_ca_layer_set_contents_scale(void *layer, double scale);
void silk_ca_canvas_layer_describe(void *handle, char *out, size_t capacity);
void silk_macos_set_title(uint32_t window, const uint8_t *title, size_t length);
void silk_macos_set_content_size(uint32_t window, double width, double height);
void silk_macos_set_min_content_size(uint32_t window, double width, double height);
void silk_macos_set_visible(uint32_t window, uint8_t visible);
void silk_macos_focus(uint32_t window);
void silk_macos_minimize(uint32_t window);
void silk_macos_toggle_full_screen(uint32_t window);
void silk_macos_start_window_drag(uint32_t window);
// Places the window controls in a bar `height` points tall at the top of the content, their
// top-left corner `left` and `top` points from the window's edges. A 0 `top` centres them in
// the bar and a 0 `left` matches the top gap; a 0 `height` leaves them where AppKit puts them.
void silk_macos_set_controls(uint32_t window, double height, double left, double top);
void silk_macos_set_cursor(uint32_t window, uint32_t cursor);
// A cursor drawn from `pixels` (width * height RGBA, not premultiplied) with its hot spot at
// (hotX, hotY) in pixels, shown until the next set_cursor or set_cursor_image. `scale` is pixels
// per point. Returns 1 when shown, 0 when the system has no custom cursors.
uint8_t silk_macos_set_cursor_image(uint32_t window, const uint8_t *pixels, uint32_t width,
                                    uint32_t height, uint32_t hotX, uint32_t hotY, double scale);
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
// Clipboard kinds: 0 text/plain, 1 text/html, 2 image/png. The kinds it holds, bit per kind.
uint32_t silk_macos_clipboard_types(void);
// The clipboard's data as `kind`, valid until the next read; NULL when it holds none. PNG is
// converted from TIFF when only that is there.
const uint8_t *silk_macos_clipboard_read_type(uint32_t kind, size_t *length);
// Clears the clipboard, then puts one representation per add on it.
void silk_macos_clipboard_begin(void);
void silk_macos_clipboard_add(uint32_t kind, const uint8_t *data, size_t length);
// 0 light, 1 dark.
uint32_t silk_macos_appearance(void);

// The tray icon. Its menu is described with the menu_* calls between tray_begin and
// tray_commit, items at the top level and begin/end for submenus; an empty menu makes clicks
// deliver TRAY_CLICKED instead. `pixels` is width * height RGBA, not premultiplied. Returns
// 1 when the icon is shown, 0 when the system has no tray.
void silk_macos_tray_begin(void);
uint8_t silk_macos_tray_commit(const uint8_t *pixels, uint32_t width, uint32_t height,
                         const uint8_t *tooltip, size_t tooltipLength, uint32_t flags);
void silk_macos_tray_remove(void);

// A context menu at (x, y) in the window's content, in points. Its items are described with
// the menu_* calls between context_menu_begin and context_menu_commit, like the tray's. A
// chosen item delivers MENU_ACTION; closing it without a choice delivers CONTEXT_MENU_CLOSED.
// Returns 1 when the menu will show.
void silk_macos_context_menu_begin(void);
uint8_t silk_macos_context_menu_commit(uint32_t window, double x, double y);

// The dock icon's badge, "" for none, and its progress bar, a negative fraction for none.
void silk_macos_set_badge(const uint8_t *text, size_t length);
void silk_macos_set_progress(double fraction);

// Opens a URL with its default app, shows a path selected in Finder, and drags files
// (paths separated by newlines) out of a window while the button is down. 1 on success.
uint8_t silk_macos_open_url(const uint8_t *url, size_t length);
uint8_t silk_macos_reveal_path(const uint8_t *path, size_t length);
uint8_t silk_macos_start_file_drag(uint32_t window, const uint8_t *paths, size_t length);

// A shortcut that delivers GLOBAL_SHORTCUT with `id` whichever app has focus: the key with
// KeyCode `code` and exactly the modifier bits `modifiers`. Registering an id again replaces
// it. Returns 1 when registered, 0 when the system refused or has no global shortcuts.
uint8_t silk_macos_shortcut_register(uint32_t id, uint32_t code, uint32_t modifiers);
void silk_macos_shortcut_unregister(uint32_t id);

// Shows a desktop notification. Returns its id (NOTIFICATION_CLICKED carries it), or 0 when
// it could not be shown.
uint32_t silk_macos_notify(const uint8_t *title, size_t titleLength, const uint8_t *body,
                     size_t bodyLength);

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
// Drags files over the window as another app would. kind 0 enters, 1 moves, 2 leaves, 3
// drops; `paths` are separated by newlines. x, y in points of the content area.
uint8_t silk_macos_test_drop(uint32_t window, uint32_t kind, double x, double y,
                         const uint8_t *paths, size_t length);
