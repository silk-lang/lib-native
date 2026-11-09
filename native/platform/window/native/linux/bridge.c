// The Linux backend of native/platform/window: native/bridge.h over Wayland or X11.
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
        int count = 1 + silk_dialog_fds(fds + 1, 15);
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

uint32_t silk_window_open_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent) {
    (void)parent;
    return silk_dialog_open(title, titleLength, directory, directoryLength, extensions,
                            extensionsLength, flags, false, NULL, 0);
}

uint32_t silk_window_save_dialog(const uint8_t *title, size_t titleLength,
                                 const uint8_t *directory, size_t directoryLength,
                                 const uint8_t *name, size_t nameLength,
                                 const uint8_t *extensions, size_t extensionsLength,
                                 uint32_t flags, uint32_t parent) {
    (void)parent;
    return silk_dialog_open(title, titleLength, directory, directoryLength, extensions,
                            extensionsLength, flags, true, name, nameLength);
}

int64_t silk_window_clipboard_read(uint8_t *destination, size_t capacity) {
    return backend ? DISPATCH(clipboard_read)(destination, capacity) : -1;
}

void silk_window_clipboard_write(const uint8_t *text, size_t length) {
    if (backend) DISPATCH(clipboard_write)(text, length);
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
