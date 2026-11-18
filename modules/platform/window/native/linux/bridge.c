// The Linux backend of modules/platform/window: native/bridge.h over Wayland or X11.
//
// silk_window_start connects to Wayland when WAYLAND_DISPLAY is set and the compositor has
// xdg-shell, and to X11 otherwise. SILK_WINDOW_BACKEND=x11 or =wayland forces one.
//
// The event loop is the same for both: read what the connection has, queue the frames
// that are due, return the oldest queued event, and otherwise poll the connection, the
// dialog processes and the next frame time.
#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "linux.h"

static int backend = SILK_BACKEND_NONE;
static uint32_t appearanceValue;
// silk_window_wake writes here from any thread; the event loop polls it with the connection.
static int wakeFd = -1;

// Builds without the Wayland headers define SILK_WINDOW_NO_WAYLAND and get X11 only.
#ifdef SILK_WINDOW_NO_WAYLAND
#define DISPATCH(call) silk_x11_##call
#define WAYLAND_START() false
#else
#define DISPATCH(call) (backend == SILK_BACKEND_WAYLAND ? silk_wl_##call : silk_x11_##call)
#define WAYLAND_START() silk_wl_start()
#endif

static uint32_t detect_appearance(void) {
    const char *theme = getenv("GTK_THEME");
    if (theme && strstr(theme, ":dark")) return 1;
    const char *forced = getenv("SILK_WINDOW_APPEARANCE");
    if (forced) return strcmp(forced, "dark") == 0;
    // GNOME and others publish the preference in GSettings; ask once, if the tool exists.
    FILE *pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
    if (!pipe) return 0;
    char line[128] = {0};
    char *read = fgets(line, sizeof line, pipe);
    pclose(pipe);
    return read && strstr(line, "dark") ? 1 : 0;
}

int32_t silk_window_start(void) {
    if (backend != SILK_BACKEND_NONE) return 1;
    const char *forced = getenv("SILK_WINDOW_BACKEND");
    bool tryWayland = !forced || strcmp(forced, "wayland") == 0;
    bool tryX11 = !forced || strcmp(forced, "x11") == 0;
    if (tryWayland && getenv("WAYLAND_DISPLAY") && WAYLAND_START()) {
        backend = SILK_BACKEND_WAYLAND;
    } else if (tryX11 && silk_x11_start()) {
        backend = SILK_BACKEND_X11;
    } else {
        return 3;
    }
    appearanceValue = detect_appearance();
    wakeFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    return 0;
}

void silk_window_wake(void) {
    if (wakeFd < 0) return;
    uint64_t one = 1;
    ssize_t written = write(wakeFd, &one, sizeof one);
    (void)written;
}

// Whether a wake came, consuming it.
static bool take_wake(void) {
    if (wakeFd < 0) return false;
    uint64_t count = 0;
    return read(wakeFd, &count, sizeof count) == (ssize_t)sizeof count && count > 0;
}

uint32_t silk_window_backend(void) { return (uint32_t)backend; }

uint32_t silk_linux_appearance(void) { return appearanceValue; }

uint32_t silk_window_appearance(void) { return appearanceValue; }

uint32_t silk_window_open(const uint8_t *title, size_t titleLength, double width, double height,
                          double minWidth, double minHeight, uint32_t flags) {
    if (!backend) return 0;
    return DISPATCH(open)(title, titleLength, width, height, minWidth, minHeight, flags);
}

void silk_window_close(uint32_t window) {
    if (!backend) return;
    DISPATCH(close)(window);
    silk_queue_forget_window(window);
}

uint8_t silk_window_next_event(double timeout, SilkWindowEvent *event) {
    if (!backend) return 0;
    double deadline = timeout < 0 ? INFINITY : silk_now() + timeout;
    for (;;) {
        DISPATCH(dispatch)();
        silk_dialog_dispatch();
        silk_dbus_dispatch();
        double now = silk_now();
        double nextFrame = DISPATCH(frames)(now);
        if (silk_queue_pop(event)) return 1;
        if (take_wake()) return 0;
        now = silk_now();
        if (now >= deadline) return 0;
        double wake = deadline < nextFrame ? deadline : nextFrame;
        int milliseconds = -1;
        if (isfinite(wake)) {
            double wait = (wake - now) * 1000.0;
            milliseconds = wait <= 0 ? 0 : (int)ceil(wait);
        }
        if (!DISPATCH(prepare_wait)()) continue;
        struct pollfd fds[17];
        fds[0].fd = DISPATCH(fd)();
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        int count = 1 + silk_dialog_fds(fds + 1, 14);
        if (silk_dbus_fd() >= 0) {
            fds[count].fd = silk_dbus_fd();
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }
        if (wakeFd >= 0) {
            fds[count].fd = wakeFd;
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }
        int ready = poll(fds, (nfds_t)count, milliseconds);
        if (ready < 0 && errno != EINTR) {
            DISPATCH(cancel_wait)();
            return 0;
        }
        DISPATCH(finish_wait)(ready > 0 && (fds[0].revents & (POLLIN | POLLHUP | POLLERR)));
    }
}

void silk_window_request_frame(uint32_t window) {
    if (backend) DISPATCH(request_frame)(window);
}

void silk_window_content_size(uint32_t window, double *width, double *height) {
    *width = 0;
    *height = 0;
    if (backend) DISPATCH(content_size)(window, width, height);
}

double silk_window_scale_factor(uint32_t window) {
    return backend ? DISPATCH(scale_factor)(window) : 1.0;
}

uint32_t silk_window_raw_surface(uint32_t window, void **first, void **second, uint64_t *id) {
    *first = NULL;
    *second = NULL;
    *id = 0;
    return backend ? DISPATCH(raw_surface)(window, first, second, id) : SILK_SURFACE_NONE;
}

void silk_window_set_title(uint32_t window, const uint8_t *title, size_t length) {
    if (backend) DISPATCH(set_title)(window, title, length);
}

void silk_window_set_content_size(uint32_t window, double width, double height) {
    if (backend) DISPATCH(set_content_size)(window, width, height);
}

void silk_window_set_min_content_size(uint32_t window, double width, double height) {
    if (backend) DISPATCH(set_min_content_size)(window, width, height);
}

void silk_window_set_visible(uint32_t window, uint8_t visible) {
    if (backend) DISPATCH(set_visible)(window, visible != 0);
}

void silk_window_focus(uint32_t window) {
    if (backend) DISPATCH(focus)(window);
}

void silk_window_minimize(uint32_t window) {
    if (backend) DISPATCH(minimize)(window);
}

void silk_window_toggle_full_screen(uint32_t window) {
    if (backend) DISPATCH(toggle_full_screen)(window);
}

void silk_window_start_window_drag(uint32_t window) {
    if (backend) DISPATCH(start_window_drag)(window);
}

void silk_window_set_cursor(uint32_t window, uint32_t cursor) {
    if (backend) DISPATCH(set_cursor)(window, cursor);
}

const char *const *silk_cursor_names(uint32_t cursor) {
    // types.Cursor, in declaration order.
    static const char *const names[][4] = {
        {"default", "left_ptr", NULL},
        {"text", "xterm", NULL},
        {"pointer", "hand2", NULL},
        {"crosshair", "cross", NULL},
        {"grabbing", "closedhand", NULL},
        {"grab", "openhand", NULL},
        {"not-allowed", "crossed_circle", NULL},
        {"ew-resize", "sb_h_double_arrow", NULL},
        {"ns-resize", "sb_v_double_arrow", NULL},
        {"w-resize", "left_side", NULL},
        {"e-resize", "right_side", NULL},
        {"n-resize", "top_side", NULL},
        {"s-resize", "bottom_side", NULL},
        {"nwse-resize", "bd_double_arrow", "size_fdiag", NULL},
        {"nesw-resize", "fd_double_arrow", "size_bdiag", NULL},
        {"col-resize", "split_h", NULL},
        {"row-resize", "split_v", NULL},
        {"vertical-text", NULL},
        {"copy", "dnd-copy", NULL},
        {"alias", "dnd-link", NULL},
        {"context-menu", NULL},
        {NULL},
        {"wait", "watch", NULL},
        {"progress", "left_ptr_watch", NULL},
        {"help", "question_arrow", NULL},
        {"zoom-in", NULL},
        {"zoom-out", NULL},
        {"cell", "plus", NULL},
        {"no-drop", "dnd-no-drop", NULL},
        {"move", "fleur", NULL},
        {"all-scroll", "fleur", NULL},
        {"nw-resize", "top_left_corner", NULL},
        {"ne-resize", "top_right_corner", NULL},
        {"sw-resize", "bottom_left_corner", NULL},
        {"se-resize", "bottom_right_corner", NULL},
    };
    return cursor < sizeof names / sizeof names[0] ? names[cursor] : names[0];
}

int silk_cursor_theme_size(void) {
    const char *size = getenv("XCURSOR_SIZE");
    int value = size ? atoi(size) : 0;
    return value > 0 ? value : 24;
}

uint8_t silk_window_set_cursor_image(uint32_t window, const uint8_t *pixels, uint32_t width,
                                     uint32_t height, uint32_t hotX, uint32_t hotY, double scale) {
    return backend ? DISPATCH(set_cursor_image)(window, pixels, width, height, hotX, hotY, scale) : 0;
}

void silk_window_set_ime(uint32_t window, uint8_t enabled, double x, double y, double width,
                         double height) {
    if (backend) DISPATCH(set_ime)(window, enabled != 0, x, y, width, height);
}

// Linux has no native menu bar outside toolkits; the menu's shortcuts still work.
void silk_window_menu_commit(void) { silk_menu_commit_pending(); }

bool silk_linux_shortcut(uint32_t window, uint32_t character, uint32_t named,
                         uint32_t modifiers) {
    SilkShortcutMatch match = silk_menu_match(character, named, modifiers);
    if (match.type == SILK_MENU_ACTION) {
        SilkWindowEvent event = silk_event(SILK_EVENT_MENU_ACTION, 0);
        event.code = match.action;
        silk_queue_push(&event);
        return true;
    }
    if (match.type == SILK_MENU_SYSTEM) {
        silk_linux_system_item(window, match.action);
        return true;
    }
    return false;
}

void silk_linux_system_item(uint32_t window, uint32_t item) {
    SilkWindowEvent event;
    switch (item) {
    case SILK_ITEM_QUIT:
        event = silk_event(SILK_EVENT_QUIT_REQUESTED, 0);
        silk_queue_push(&event);
        break;
    case SILK_ITEM_CLOSE_WINDOW:
        event = silk_event(SILK_EVENT_CLOSE_REQUESTED, window);
        silk_queue_push(&event);
        break;
    case SILK_ITEM_MINIMIZE:
        silk_window_minimize(window);
        break;
    case SILK_ITEM_TOGGLE_FULL_SCREEN:
        silk_window_toggle_full_screen(window);
        break;
    default:
        break;
    }
}

// The portal's name for a window: "x11:<xid>" on X11. Wayland would need xdg-foreign to
// export a handle, so there the dialog has no parent.
static void dialog_parent(uint32_t window, char *out, size_t capacity) {
    out[0] = 0;
    void *first = NULL, *second = NULL;
    uint64_t xid = 0;
    if (window && backend == SILK_BACKEND_X11 &&
        silk_x11_raw_surface(window, &first, &second, &xid) == SILK_SURFACE_XLIB)
        snprintf(out, capacity, "x11:%llx", (unsigned long long)xid);
}

uint32_t silk_window_open_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent) {
    char parentName[40];
    dialog_parent(parent, parentName, sizeof parentName);
    return silk_dialog_open(title, titleLength, directory, directoryLength, extensions,
                            extensionsLength, flags, false, NULL, 0, parentName);
}

uint32_t silk_window_save_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *name, size_t nameLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent) {
    char parentName[40];
    dialog_parent(parent, parentName, sizeof parentName);
    return silk_dialog_open(title, titleLength, directory, directoryLength, extensions,
                            extensionsLength, flags, true, name, nameLength, parentName);
}

int64_t silk_window_clipboard_read(uint8_t *destination, size_t capacity) {
    return backend ? DISPATCH(clipboard_read)(destination, capacity) : -1;
}

void silk_window_clipboard_write(const uint8_t *text, size_t length) {
    if (backend) DISPATCH(clipboard_write)(text, length);
}

int64_t silk_window_clipboard_types(uint8_t *destination, size_t capacity) {
    return backend ? DISPATCH(clipboard_types)(destination, capacity) : -1;
}

int64_t silk_window_clipboard_read_type(const uint8_t *mime, size_t mimeLength,
                                        uint8_t *destination, size_t capacity) {
    int kind = silk_clip_kind(mime, mimeLength);
    return backend && kind >= 0 ? DISPATCH(clipboard_read_type)(kind, destination, capacity) : -1;
}

void silk_window_clipboard_commit(void) {
    if (backend) DISPATCH(clipboard_commit)();
}

uint8_t silk_window_test_key(uint32_t window, uint32_t code, const uint8_t *text, size_t length,
                             uint32_t modifiers) {
    return backend ? DISPATCH(test_key)(window, code, text, length, modifiers) : 0;
}

uint8_t silk_window_test_mouse(uint32_t window, uint32_t kind, uint32_t button, double x,
                               double y, double dx, double dy) {
    return backend ? DISPATCH(test_mouse)(window, kind, button, x, y, dx, dy) : 0;
}

uint8_t silk_window_test_preedit(uint32_t window, const uint8_t *text, size_t length,
                                 uint64_t start, uint64_t end) {
    return backend ? DISPATCH(test_preedit)(window, text, length, start, end) : 0;
}

uint8_t silk_window_test_commit(uint32_t window, const uint8_t *text, size_t length) {
    return backend ? DISPATCH(test_commit)(window, text, length) : 0;
}

uint8_t silk_window_test_resize(uint32_t window, double width, double height) {
    return backend ? DISPATCH(test_resize)(window, width, height) : 0;
}

void silk_window_test_ime_rect(uint32_t window, double *x, double *y, double *width,
                               double *height) {
    *x = *y = *width = *height = 0;
    if (backend) DISPATCH(test_ime_rect)(window, x, y, width, height);
}

// ---------------------------------------------------------------------------------------
// File drags, global shortcuts, the tray and notifications.

static int hex_value(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t silk_uri_list_paths(const uint8_t *list, size_t length, SilkText *out, size_t capacity) {
    size_t count = 0, start = 0;
    for (size_t i = 0; i <= length && count < capacity; i++) {
        if (i < length && list[i] != '\n' && list[i] != '\r') continue;
        const uint8_t *line = list + start;
        size_t lineLength = i - start;
        start = i + 1;
        if (lineLength < 7 || line[0] == '#' || memcmp(line, "file://", 7) != 0) continue;
        line += 7;
        lineLength -= 7;
        // file://host/path: skip the host, which is empty or localhost.
        size_t slash = 0;
        while (slash < lineLength && line[slash] != '/') slash++;
        if (slash == lineLength) continue;
        uint8_t *path = malloc(lineLength - slash + 1);
        if (!path) abort();
        size_t pathLength = 0;
        for (size_t j = slash; j < lineLength; j++) {
            int high, low;
            if (line[j] == '%' && j + 2 < lineLength && (high = hex_value(line[j + 1])) >= 0 &&
                (low = hex_value(line[j + 2])) >= 0) {
                path[pathLength++] = (uint8_t)(high * 16 + low);
                j += 2;
            } else {
                path[pathLength++] = line[j];
            }
        }
        out[count].bytes = path;
        out[count].length = pathLength;
        count++;
    }
    return count;
}

char *silk_paths_uri_list(const uint8_t *paths, size_t length, size_t *listLength) {
    // Every byte may grow to %XX, plus "file://" and CRLF per path.
    char *list = malloc(length * 3 + (length + 1) * 9 + 1);
    if (!list) abort();
    size_t at = 0;
    SilkText split[SILK_MAX_TEXTS];
    size_t count = silk_split_paths(paths, length, split, SILK_MAX_TEXTS);
    for (size_t i = 0; i < count; i++) {
        memcpy(list + at, "file://", 7);
        at += 7;
        for (size_t j = 0; j < split[i].length; j++) {
            uint8_t c = split[i].bytes[j];
            bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                         strchr("/-._~", c);
            if (plain && c) {
                list[at++] = (char)c;
            } else {
                at += (size_t)sprintf(list + at, "%%%02X", c);
            }
        }
        list[at++] = '\r';
        list[at++] = '\n';
    }
    list[at] = 0;
    *listLength = at;
    return list;
}

void silk_free_texts(SilkText *texts, size_t count) {
    for (size_t i = 0; i < count; i++) free(texts[i].bytes);
}

uint8_t silk_window_test_drop(uint32_t window, uint32_t kind, double x, double y,
                              const uint8_t *paths, size_t length) {
    if (backend != SILK_BACKEND_X11) return 0;
    return silk_x11_test_drop(window, kind, x, y, paths, length);
}

uint8_t silk_window_shortcut_register(uint32_t id, uint32_t code, uint32_t modifiers) {
    // Wayland leaves global shortcuts to the desktop portal, which asks the user to bind them.
    if (backend != SILK_BACKEND_X11) return 0;
    return silk_x11_shortcut_register(id, code, modifiers);
}

void silk_window_shortcut_unregister(uint32_t id) {
    if (backend == SILK_BACKEND_X11) silk_x11_shortcut_unregister(id);
}
