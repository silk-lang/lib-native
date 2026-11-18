// Internal interface between the Linux bridge (bridge.c) and its two window systems.
#pragma once
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include "../common/core.h"
#include "keys.h"

// Key helpers shared by X11 and Wayland, which both name keys with XKB keysyms.
// The NamedKey a keysym produces, or 0 when it types a character instead.
uint32_t silk_keysym_named(uint32_t keysym);
// The KeyCode of an evdev key code (X11 key code - 8).
uint32_t silk_evdev_code(uint32_t evdev);
// The evdev key code of a KeyCode, or 0.
uint32_t silk_evdev_from_code(uint32_t code);

// The cursor theme names of types.Cursor `cursor`, best first and ending in NULL: the CSS
// keyword, then older names themes use.
const char *const *silk_cursor_names(uint32_t cursor);
// The theme's cursor size in pixels, from XCURSOR_SIZE, 24 when unset.
int silk_cursor_theme_size(void);
// The modifier bit a NamedKey stands for, or 0.
uint32_t silk_named_modifier(uint32_t named);

// Frame pacing shared by both: a window asks with request_frame, a forced frame (open,
// show, resize, scale change) is delivered at once.
typedef struct SilkFrameState {
    bool pending;
    bool forced;
    bool waiting; // Wayland: a frame callback is outstanding
} SilkFrameState;

// File-chooser dialogs (dialog.c): the FileChooser portal when the session has one, else a
// zenity or kdialog process. `parent` names the window for the portal ("x11:<xid>") or is "".
uint32_t silk_dialog_open(const uint8_t *title, size_t titleLength, const uint8_t *directory,
                          size_t directoryLength, const uint8_t *extensions,
                          size_t extensionsLength, uint32_t flags, bool save,
                          const uint8_t *name, size_t nameLength, const char *parent);
// Asks the FileChooser portal (dbus.c) for dialog `dialog`. False when there is no portal that
// can show it, or SILK_WINDOW_PORTAL=0; then nothing was sent.
bool silk_portal_dialog(uint32_t dialog, bool save, const char *title, const char *directory,
                        const char *name, const char *label, char **extensions,
                        size_t extensionCount, bool directories, bool multiple,
                        const char *parent);
// Adds the running dialogs' pipes to `fds` and returns how many it added.
int silk_dialog_fds(struct pollfd *fds, int capacity);
// Reads finished dialogs and queues DialogClosed for them.
void silk_dialog_dispatch(void);

// Each window system implements these with its prefix (silk_x11_, silk_wl_).
#define SILK_LINUX_BACKEND(prefix)                                                          \
    bool prefix##start(void);                                                               \
    uint32_t prefix##open(const uint8_t *title, size_t titleLength, double width,           \
                          double height, double minWidth, double minHeight, uint32_t flags);\
    void prefix##close(uint32_t window);                                                    \
    /* Reads events already available without blocking and queues them. */                 \
    void prefix##dispatch(void);                                                            \
    /* The connection's file descriptor to poll for input. */                               \
    int prefix##fd(void);                                                                   \
    /* Before blocking: flush requests. Returns false if events are already buffered. */    \
    bool prefix##prepare_wait(void);                                                        \
    void prefix##cancel_wait(void);                                                         \
    /* After poll said the fd is readable (or not). */                                      \
    void prefix##finish_wait(bool readable);                                                \
    /* Queues the frames that are due and returns when the next one is due (or INFINITY).*/ \
    double prefix##frames(double now);                                                      \
    void prefix##request_frame(uint32_t window);                                            \
    void prefix##content_size(uint32_t window, double *width, double *height);              \
    double prefix##scale_factor(uint32_t window);                                           \
    uint32_t prefix##raw_surface(uint32_t window, void **first, void **second, uint64_t *id);\
    void prefix##set_title(uint32_t window, const uint8_t *title, size_t length);           \
    void prefix##set_content_size(uint32_t window, double width, double height);            \
    void prefix##set_min_content_size(uint32_t window, double width, double height);        \
    void prefix##set_visible(uint32_t window, bool visible);                                \
    void prefix##focus(uint32_t window);                                                    \
    void prefix##minimize(uint32_t window);                                                 \
    void prefix##toggle_full_screen(uint32_t window);                                       \
    void prefix##start_window_drag(uint32_t window);                                        \
    void prefix##set_cursor(uint32_t window, uint32_t cursor);                              \
    uint8_t prefix##set_cursor_image(uint32_t window, const uint8_t *pixels, uint32_t width, \
                                     uint32_t height, uint32_t hotX, uint32_t hotY,         \
                                     double scale);                                         \
    void prefix##set_ime(uint32_t window, bool enabled, double x, double y, double width,   \
                         double height);                                                    \
    int64_t prefix##clipboard_read(uint8_t *destination, size_t capacity);                  \
    void prefix##clipboard_write(const uint8_t *text, size_t length);                       \
    int64_t prefix##clipboard_types(uint8_t *destination, size_t capacity);                 \
    int64_t prefix##clipboard_read_type(int kind, uint8_t *destination, size_t capacity);   \
    void prefix##clipboard_commit(void);                                                    \
    uint8_t prefix##test_key(uint32_t window, uint32_t code, const uint8_t *text,           \
                             size_t length, uint32_t modifiers);                            \
    uint8_t prefix##test_mouse(uint32_t window, uint32_t kind, uint32_t button, double x,   \
                               double y, double dx, double dy);                             \
    uint8_t prefix##test_preedit(uint32_t window, const uint8_t *text, size_t length,       \
                                 uint64_t start, uint64_t end);                             \
    uint8_t prefix##test_commit(uint32_t window, const uint8_t *text, size_t length);       \
    uint8_t prefix##test_resize(uint32_t window, double width, double height);              \
    void prefix##test_ime_rect(uint32_t window, double *x, double *y, double *width,        \
                               double *height);

SILK_LINUX_BACKEND(silk_x11_)

// X11 only: XDND as the source for test_drop, and keys grabbed on the root window.
uint8_t silk_x11_test_drop(uint32_t window, uint32_t kind, double x, double y,
                           const uint8_t *paths, size_t length);
uint8_t silk_x11_shortcut_register(uint32_t id, uint32_t code, uint32_t modifiers);
void silk_x11_shortcut_unregister(uint32_t id);
SILK_LINUX_BACKEND(silk_wl_)

// The key-press path both window systems share once they have decoded a press: matches
// menu shortcuts (queuing MenuAction or the system item's event) and reports whether the
// press was used by one.
bool silk_linux_shortcut(uint32_t window, uint32_t character, uint32_t named,
                         uint32_t modifiers);
// Called by a window system for SystemItem shortcuts that act on a window.
void silk_linux_system_item(uint32_t window, uint32_t item);

// Paths of the file:// URIs in a text/uri-list, percent-decoded, in new memory freed with
// silk_free_texts. Returns how many (at most `capacity`); other URIs and comments are skipped.
size_t silk_uri_list_paths(const uint8_t *list, size_t length, SilkText *out, size_t capacity);
// The text/uri-list of newline-separated paths, as for test_drop. Free with free().
char *silk_paths_uri_list(const uint8_t *paths, size_t length, size_t *listLength);
void silk_free_texts(SilkText *texts, size_t count);

// The session bus (dbus.c): tray icon and notifications, the same on X11 and Wayland.
// The connection's descriptor while connected, or -1.
int silk_dbus_fd(void);
// Reads and handles what the bus sent.
void silk_dbus_dispatch(void);

// The appearance from the environment (GTK_THEME ending in ":dark"), 0 light, 1 dark.
uint32_t silk_linux_appearance(void);
