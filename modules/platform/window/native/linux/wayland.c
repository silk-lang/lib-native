// The Wayland half of the Linux backend.
//
// - Windows are xdg-shell toplevels. Server-side decorations are asked for through
//   xdg-decoration; compositors without them (GNOME, Weston) get a small client-side title
//   bar drawn into a subsurface, with close, maximize and minimize buttons, dragging and
//   double-click to maximize.
// - Scale: wp_fractional_scale_v1 with a viewport when the compositor has it, otherwise
//   the integer preferred buffer scale (wl_surface v6) or the largest scale of the outputs
//   the surface is on.
// - Frames: wl_surface.frame callbacks pace requested frames once the surface has shown
//   content (the compositor stops them for hidden windows). Until a callback has ever
//   fired, as before the app's first present, a timer at 60 Hz stands in.
// - Keyboard: xkbcommon with compose (dead keys), key repeat done here at the rate the
//   compositor gives. Input methods: zwp_text_input_v3.
// - Clipboard: wl_data_device with text/plain;charset=utf-8.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>
#include "cursor-shape-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "linux.h"
#include "text-input-unstable-v3-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

enum { TITLE_HEIGHT = 30, BUTTON_WIDTH = 40 };

typedef struct WlOutput {
    struct wl_output *output;
    uint32_t name;
    int32_t scale;
} WlOutput;

typedef struct WlWindow {
    uint32_t id;
    bool open;
    struct wl_surface *surface;
    struct xdg_surface *xdgSurface;
    struct xdg_toplevel *toplevel;
    struct zxdg_toplevel_decoration_v1 *decoration;
    struct wp_fractional_scale_v1 *fractional;
    struct wp_viewport *viewport;
    // Client-side title bar.
    bool csd;
    bool titleHidden;
    struct wl_surface *titleSurface;
    struct wl_subsurface *titleSubsurface;
    struct wl_buffer *titleBuffer;
    int titleBufferWidth;
    int titleBufferScale;
    int hoveredButton; // 0 none, 1 close, 2 maximize, 3 minimize
    // State.
    bool configured;
    bool visible;
    bool activated;
    bool maximized;
    bool fullScreen;
    bool resizable;
    double width, height;          // content size in points
    double pendingWidth, pendingHeight;
    double minWidth, minHeight;
    double scale;                  // device pixels per point
    int32_t preferredBufferScale;  // wl_surface v6, 0 when not sent
    uint32_t fractional120;        // fractional scale, 0 when not sent
    WlOutput *outputs[8];
    int outputCount;
    SilkFrameState frame;
    struct wl_callback *callback;
    bool callbackEverFired;
    double due;
    uint32_t cursor;
    bool imeEnabled;
    double imeX, imeY, imeWidth, imeHeight;
    char *title;
} WlWindow;

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static uint32_t compositorVersion;
static struct wl_subcompositor *subcompositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wmBase;
static struct zxdg_decoration_manager_v1 *decorationManager;
static struct wp_fractional_scale_manager_v1 *fractionalManager;
static struct wp_viewporter *viewporter;
static struct wp_cursor_shape_manager_v1 *cursorShapeManager;
static struct zwp_text_input_manager_v3 *textInputManager;
static struct wl_data_device_manager *dataDeviceManager;
static struct wl_seat *seat;
static struct wl_keyboard *keyboard;
static struct wl_pointer *pointer;
static struct wp_cursor_shape_device_v1 *cursorShapeDevice;
static struct zwp_text_input_v3 *textInput;
static struct wl_data_device *dataDevice;
static struct wl_cursor_theme *cursorTheme;
static struct wl_surface *cursorSurface;
static WlOutput outputs[16];
static int outputCount;

static WlWindow **windows; // each window stays put: listeners hold its address
static size_t windowCount, windowCapacity;
static uint32_t nextWindow = 1;
static double refreshInterval = 1.0 / 60.0;

// Keyboard.
static struct xkb_context *xkb;
static struct xkb_keymap *keymap;
static struct xkb_state *xkbState;
static struct xkb_state *shiftState;
static struct xkb_compose_table *composeTable;
static struct xkb_compose_state *composeState;
static uint32_t keyboardFocus;
static uint32_t modifierBits;
static int32_t repeatRate = 25, repeatDelay = 400;
static uint32_t repeatKey;
static double repeatNext;
static uint32_t lastSerial;
static uint8_t pressed[64];

// Pointer.
static uint32_t pointerFocus;
static bool pointerOnTitle;
static double pointerX, pointerY;
static uint32_t pointerEnterSerial;
static double lastClickTime;
static double lastClickX, lastClickY;
static uint32_t lastClickButton, clickCount;
static double axisX, axisY;
static int32_t axis120X, axis120Y;
static bool axisDiscrete;

// Text input.
static uint32_t textFocus;
static char *pendingPreedit;
static int32_t pendingPreeditBegin, pendingPreeditEnd;
static char *pendingCommit;
static bool textInputActive;

// Clipboard.
static struct wl_data_source *ownSource;
static uint8_t *ownText;
static size_t ownLength;
static struct wl_data_offer *selectionOffer;
static bool selectionOfferHasText;
static uint8_t *readText;
static int64_t readLength = -1;

static WlWindow *find(uint32_t id) {
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->id == id && windows[i]->open) return windows[i];
    return NULL;
}

static WlWindow *find_surface(struct wl_surface *surface) {
    if (!surface) return NULL;
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->open && (windows[i]->surface == surface || windows[i]->titleSurface == surface))
            return windows[i];
    return NULL;
}

static double title_height(WlWindow *window) {
    return window->csd && !window->fullScreen ? TITLE_HEIGHT : 0;
}

// ---------------------------------------------------------------------------------------
// Shared memory buffers for the title bar and cursors.

static int create_shm_file(size_t size) {
    int fd = memfd_create("silk-window", MFD_CLOEXEC);
    if (fd < 0) return -1;
    if (ftruncate(fd, (off_t)size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void buffer_release(void *data, struct wl_buffer *buffer) {
    (void)data;
    wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener bufferListener = {buffer_release};

static void fill_rect(uint32_t *pixels, int stride, int width, int height, int x0, int y0,
                      int x1, int y1, uint32_t color) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) pixels[y * stride + x] = color;
}

static void draw_glyph(uint32_t *pixels, int stride, int width, int height, int cx, int cy,
                       int size, int kind, uint32_t color, int thickness) {
    // 1 close (an X), 2 maximize (a square), 3 minimize (a bar).
    if (kind == 1) {
        for (int i = -size; i <= size; i++)
            for (int t = 0; t < thickness; t++) {
                fill_rect(pixels, stride, width, height, cx + i, cy + i + t, cx + i + 1, cy + i + t + 1, color);
                fill_rect(pixels, stride, width, height, cx + i, cy - i + t, cx + i + 1, cy - i + t + 1, color);
            }
    } else if (kind == 2) {
        fill_rect(pixels, stride, width, height, cx - size, cy - size, cx + size, cy - size + thickness, color);
        fill_rect(pixels, stride, width, height, cx - size, cy + size - thickness, cx + size, cy + size, color);
        fill_rect(pixels, stride, width, height, cx - size, cy - size, cx - size + thickness, cy + size, color);
        fill_rect(pixels, stride, width, height, cx + size - thickness, cy - size, cx + size, cy + size, color);
    } else {
        fill_rect(pixels, stride, width, height, cx - size, cy + size - thickness, cx + size, cy + size, color);
    }
}

static void draw_title(WlWindow *window) {
    if (!window->csd || !window->titleSurface || !shm) return;
    if (window->fullScreen) {
        wl_surface_attach(window->titleSurface, NULL, 0, 0);
        wl_surface_commit(window->titleSurface);
        return;
    }
    int bufferScale = (int)ceil(window->scale);
    if (bufferScale < 1) bufferScale = 1;
    int width = (int)lround(window->width) * bufferScale;
    int height = TITLE_HEIGHT * bufferScale;
    if (width <= 0) return;
    int stride = width * 4;
    size_t size = (size_t)stride * (size_t)height;
    int fd = create_shm_file(size);
    if (fd < 0) return;
    uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        close(fd);
        return;
    }
    bool dark = silk_linux_appearance() == 1;
    uint32_t background = window->activated ? (dark ? 0xFF303030 : 0xFFEBEBEB)
                                            : (dark ? 0xFF242424 : 0xFFF6F6F6);
    uint32_t foreground = dark ? 0xFFE0E0E0 : 0xFF303030;
    uint32_t hover = dark ? 0xFF484848 : 0xFFD6D6D6;
    fill_rect(pixels, width, width, height, 0, 0, width, height, background);
    for (int button = 1; button <= 3; button++) {
        int right = width - (button - 1) * BUTTON_WIDTH * bufferScale;
        int left = right - BUTTON_WIDTH * bufferScale;
        if (!window->resizable && button == 2) continue;
        if (window->hoveredButton == button)
            fill_rect(pixels, width, width, height, left, 0, right, height,
                      button == 1 ? 0xFFC42B1C : hover);
        draw_glyph(pixels, width, width, height, (left + right) / 2, height / 2, 5 * bufferScale,
                   button, window->hoveredButton == 1 && button == 1 ? 0xFFFFFFFF : foreground,
                   bufferScale);
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
                                                         WL_SHM_FORMAT_ARGB8888);
    wl_buffer_add_listener(buffer, &bufferListener, NULL);
    wl_shm_pool_destroy(pool);
    munmap(pixels, size);
    close(fd);
    wl_surface_set_buffer_scale(window->titleSurface, bufferScale);
    wl_surface_attach(window->titleSurface, buffer, 0, 0);
    wl_surface_damage_buffer(window->titleSurface, 0, 0, width, height);
    wl_surface_commit(window->titleSurface);
}

static void place_title(WlWindow *window) {
    if (!window->csd || !window->titleSubsurface) return;
    wl_subsurface_set_position(window->titleSubsurface, 0, -TITLE_HEIGHT);
}

static void set_geometry(WlWindow *window) {
    double title = title_height(window);
    xdg_surface_set_window_geometry(window->xdgSurface, 0, (int32_t)-title,
                                    (int32_t)lround(window->width),
                                    (int32_t)lround(window->height + title));
}

// ---------------------------------------------------------------------------------------
// Scale.

static double compute_scale(WlWindow *window) {
    const char *forced = getenv("SILK_WINDOW_SCALE");
    if (forced && atof(forced) > 0) return atof(forced);
    if (window->fractional120) return window->fractional120 / 120.0;
    if (window->preferredBufferScale > 0) return window->preferredBufferScale;
    int32_t best = 1;
    for (int i = 0; i < window->outputCount; i++)
        if (window->outputs[i]->scale > best) best = window->outputs[i]->scale;
    return best;
}

static void push_resized(WlWindow *window) {
    SilkWindowEvent event = silk_event(SILK_EVENT_RESIZED, window->id);
    event.x = window->width;
    event.y = window->height;
    event.value = window->scale;
    silk_queue_push(&event);
    window->frame.pending = true;
    window->frame.forced = true;
}

static void apply_scale(WlWindow *window) {
    if (window->viewport) {
        wl_surface_set_buffer_scale(window->surface, 1);
        if (window->width > 0 && window->height > 0)
            wp_viewport_set_destination(window->viewport, (int32_t)lround(window->width),
                                        (int32_t)lround(window->height));
    } else {
        wl_surface_set_buffer_scale(window->surface, (int32_t)lround(window->scale));
    }
}

static void update_scale(WlWindow *window) {
    double scale = compute_scale(window);
    if (fabs(scale - window->scale) < 1e-9) return;
    window->scale = scale;
    apply_scale(window);
    if (!window->configured) return;
    SilkWindowEvent event = silk_event(SILK_EVENT_SCALE_FACTOR_CHANGED, window->id);
    event.value = scale;
    silk_queue_push(&event);
    push_resized(window);
    draw_title(window);
}

// ---------------------------------------------------------------------------------------
// Outputs.

static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
                            int32_t width, int32_t height, int32_t subpixel, const char *make,
                            const char *model, int32_t transform) {}
static void output_mode(void *data, struct wl_output *output, uint32_t flags, int32_t width,
                        int32_t height, int32_t refresh) {
    if ((flags & WL_OUTPUT_MODE_CURRENT) && refresh > 0 && !getenv("SILK_WINDOW_REFRESH_HZ"))
        refreshInterval = 1000.0 / refresh;
}
static void output_done(void *data, struct wl_output *output) {
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i]->open) update_scale(windows[i]);
}
static void output_scale(void *data, struct wl_output *output, int32_t factor) {
    ((WlOutput *)data)->scale = factor;
}
static void output_name(void *data, struct wl_output *output, const char *name) {}
static void output_description(void *data, struct wl_output *output, const char *text) {}

static const struct wl_output_listener outputListener = {
    output_geometry, output_mode, output_done, output_scale, output_name, output_description,
};

// ---------------------------------------------------------------------------------------
// xdg-shell.

static void wm_ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener wmBaseListener = {wm_ping};

static void surface_configure(void *data, struct xdg_surface *xdgSurface, uint32_t serial) {
    WlWindow *window = data;
    xdg_surface_ack_configure(xdgSurface, serial);
    double width = window->pendingWidth, height = window->pendingHeight;
    bool first = !window->configured;
    window->configured = true;
    if (width > 0 && height > 0) {
        height -= title_height(window);
        if (width < window->minWidth) width = window->minWidth;
        if (height < window->minHeight) height = window->minHeight;
        if (height < 1) height = 1;
    } else {
        width = window->width;
        height = window->height;
    }
    bool changed = fabs(width - window->width) > 1e-9 || fabs(height - window->height) > 1e-9;
    window->width = width;
    window->height = height;
    apply_scale(window);
    set_geometry(window);
    place_title(window);
    draw_title(window);
    wl_surface_commit(window->surface);
    if (first || changed) push_resized(window);
}

static const struct xdg_surface_listener xdgSurfaceListener = {surface_configure};

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width,
                               int32_t height, struct wl_array *states) {
    WlWindow *window = data;
    window->pendingWidth = width;
    window->pendingHeight = height;
    bool activated = false, maximized = false, fullScreen = false;
    uint32_t *state;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) activated = true;
        if (*state == XDG_TOPLEVEL_STATE_MAXIMIZED) maximized = true;
        if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) fullScreen = true;
    }
    window->maximized = maximized;
    if (activated != window->activated) {
        window->activated = activated;
        SilkWindowEvent event = silk_event(SILK_EVENT_FOCUSED, window->id);
        if (activated) event.flags |= SILK_FLAG_ON;
        silk_queue_push(&event);
    }
    if (fullScreen != window->fullScreen) {
        window->fullScreen = fullScreen;
        SilkWindowEvent event = silk_event(SILK_EVENT_FULL_SCREEN_CHANGED, window->id);
        if (fullScreen) event.flags |= SILK_FLAG_ON;
        silk_queue_push(&event);
    }
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    WlWindow *window = data;
    SilkWindowEvent event = silk_event(SILK_EVENT_CLOSE_REQUESTED, window->id);
    silk_queue_push(&event);
}

static void toplevel_bounds(void *data, struct xdg_toplevel *toplevel, int32_t width,
                            int32_t height) {}
static void toplevel_capabilities(void *data, struct xdg_toplevel *toplevel,
                                  struct wl_array *capabilities) {}

static const struct xdg_toplevel_listener toplevelListener = {
    toplevel_configure, toplevel_close, toplevel_bounds, toplevel_capabilities,
};

static void decoration_configure(void *data, struct zxdg_toplevel_decoration_v1 *decoration,
                                 uint32_t mode) {
    WlWindow *window = data;
    bool csd = mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE && !window->titleHidden;
    if (csd == window->csd) return;
    window->csd = csd;
    if (!csd && window->titleSurface) {
        wl_surface_attach(window->titleSurface, NULL, 0, 0);
        wl_surface_commit(window->titleSurface);
    }
}

static const struct zxdg_toplevel_decoration_v1_listener decorationListener = {decoration_configure};

static void fractional_preferred(void *data, struct wp_fractional_scale_v1 *fractional,
                                 uint32_t scale) {
    WlWindow *window = data;
    window->fractional120 = scale;
    update_scale(window);
}

static const struct wp_fractional_scale_v1_listener fractionalListener = {fractional_preferred};

static void surface_enter(void *data, struct wl_surface *surface, struct wl_output *output) {
    WlWindow *window = data;
    for (int i = 0; i < outputCount; i++)
        if (outputs[i].output == output && window->outputCount < 8)
            window->outputs[window->outputCount++] = &outputs[i];
    update_scale(window);
}

static void surface_leave(void *data, struct wl_surface *surface, struct wl_output *output) {
    WlWindow *window = data;
    for (int i = 0; i < window->outputCount; i++)
        if (window->outputs[i]->output == output) {
            window->outputs[i] = window->outputs[--window->outputCount];
            break;
        }
    update_scale(window);
}

static void surface_preferred_scale(void *data, struct wl_surface *surface, int32_t factor) {
    WlWindow *window = data;
    window->preferredBufferScale = factor;
    update_scale(window);
}

static void surface_preferred_transform(void *data, struct wl_surface *surface,
                                        uint32_t transform) {}

static const struct wl_surface_listener surfaceListener = {
    surface_enter, surface_leave, surface_preferred_scale, surface_preferred_transform,
};

// ---------------------------------------------------------------------------------------
// Keyboard.

static void push_modifiers(uint32_t window) {
    SilkWindowEvent event = silk_event(SILK_EVENT_MODIFIERS_CHANGED, window);
    event.modifiers = modifierBits;
    silk_queue_push(&event);
}

static uint32_t current_modifiers(void) {
    if (!xkbState) return 0;
    uint32_t bits = 0;
    if (xkb_state_mod_name_is_active(xkbState, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) bits |= SILK_MOD_SHIFT;
    if (xkb_state_mod_name_is_active(xkbState, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0) bits |= SILK_MOD_CONTROL;
    if (xkb_state_mod_name_is_active(xkbState, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) bits |= SILK_MOD_ALT;
    if (xkb_state_mod_name_is_active(xkbState, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0) bits |= SILK_MOD_META;
    if (xkb_state_mod_name_is_active(xkbState, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED) > 0) bits |= SILK_MOD_CAPS_LOCK;
    return bits;
}

static void use_keymap(struct xkb_keymap *map) {
    if (xkbState) xkb_state_unref(xkbState);
    if (shiftState) xkb_state_unref(shiftState);
    if (keymap) xkb_keymap_unref(keymap);
    keymap = map;
    xkbState = xkb_state_new(keymap);
    shiftState = xkb_state_new(keymap);
}

static void ensure_keymap(void) {
    if (keymap || !xkb) return;
    struct xkb_keymap *map = xkb_keymap_new_from_names(xkb, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (map) use_keymap(map);
}

static size_t keysym_text(xkb_keysym_t keysym, uint8_t out[16]) {
    char buffer[16];
    int length = xkb_keysym_to_utf8(keysym, buffer, sizeof buffer);
    if (length <= 1) return 0;
    memcpy(out, buffer, (size_t)length - 1);
    return (size_t)length - 1;
}

static void push_text(uint32_t window, const uint8_t *text, size_t length) {
    if (!length || silk_utf8_is_control(text, length)) return;
    SilkWindowEvent event = silk_event(SILK_EVENT_TEXT_INPUT, window);
    silk_queue_push_text(&event, text, length);
}

static void key_down(uint32_t evdev, bool repeat) {
    WlWindow *window = find(keyboardFocus);
    if (!window || !xkbState) return;
    xkb_keycode_t keycode = evdev + 8;
    uint32_t code = silk_evdev_code(evdev);
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkbState, keycode);
    xkb_layout_index_t layout = xkb_state_key_get_layout(xkbState, keycode);
    const xkb_keysym_t *base = NULL;
    int baseCount = xkb_keymap_key_get_syms_by_level(keymap, keycode, layout, 0, &base);
    xkb_keysym_t baseSym = baseCount > 0 ? base[0] : XKB_KEY_NoSymbol;
    uint32_t named = silk_keysym_named(keysym);
    if (!named) named = silk_keysym_named(baseSym);
    if (!named && code < SILK_CODE_COUNT) named = silk_code_named_keys[code];
    uint8_t keyText[16];
    size_t keyTextLength = 0;
    if (!named) {
        xkb_keysym_t shifted = xkb_state_key_get_one_sym(shiftState, keycode);
        keyTextLength = keysym_text(shifted ? shifted : baseSym, keyText);
    }
    uint32_t modifiers = modifierBits;
    if (!repeat && (modifiers & (SILK_MOD_CONTROL | SILK_MOD_META | SILK_MOD_ALT))) {
        uint8_t baseText[16];
        size_t baseLength = keysym_text(baseSym, baseText);
        if (silk_linux_shortcut(window->id, baseLength ? silk_utf8_first(baseText, baseLength) : 0,
                                named, modifiers))
            return;
    }
    SilkWindowEvent event = silk_event(SILK_EVENT_KEY_DOWN, window->id);
    event.code = code;
    event.named = named;
    event.modifiers = modifiers;
    if (repeat) event.flags |= SILK_FLAG_REPEAT;
    silk_queue_push_text(&event, keyText, keyTextLength);
    if (modifiers & (SILK_MOD_CONTROL | SILK_MOD_META)) return;
    // The typed text, through compose for dead keys.
    uint8_t typed[64];
    size_t typedLength = 0;
    if (composeState && !repeat) {
        xkb_compose_state_feed(composeState, keysym);
        enum xkb_compose_status status = xkb_compose_state_get_status(composeState);
        if (status == XKB_COMPOSE_COMPOSING) return;
        if (status == XKB_COMPOSE_COMPOSED) {
            int length = xkb_compose_state_get_utf8(composeState, (char *)typed, sizeof typed);
            xkb_compose_state_reset(composeState);
            if (length > 0) push_text(window->id, typed, (size_t)length);
            return;
        }
        if (status == XKB_COMPOSE_CANCELLED) {
            xkb_compose_state_reset(composeState);
            return;
        }
    }
    int length = xkb_state_key_get_utf8(xkbState, keycode, (char *)typed, sizeof typed);
    if (length > 0) typedLength = (size_t)length;
    push_text(window->id, typed, typedLength);
}

static void key_event(uint32_t evdev, bool down) {
    WlWindow *window = find(keyboardFocus);
    if (!window) return;
    if (evdev < sizeof pressed * 8) {
        if (down) pressed[evdev / 8] |= (uint8_t)(1u << (evdev % 8));
        else pressed[evdev / 8] &= (uint8_t)~(1u << (evdev % 8));
    }
    if (down) {
        key_down(evdev, false);
        if (keymap && xkb_keymap_key_repeats(keymap, evdev + 8) && repeatRate > 0) {
            repeatKey = evdev;
            repeatNext = silk_now() + repeatDelay / 1000.0;
        }
        return;
    }
    if (repeatKey == evdev) repeatKey = 0;
    uint32_t code = silk_evdev_code(evdev);
    xkb_keysym_t keysym = xkbState ? xkb_state_key_get_one_sym(xkbState, evdev + 8) : 0;
    uint32_t named = silk_keysym_named(keysym);
    if (!named && code < SILK_CODE_COUNT) named = silk_code_named_keys[code];
    uint8_t keyText[16];
    size_t keyTextLength = 0;
    if (!named && shiftState) keyTextLength = keysym_text(xkb_state_key_get_one_sym(shiftState, evdev + 8), keyText);
    SilkWindowEvent event = silk_event(SILK_EVENT_KEY_UP, window->id);
    event.code = code;
    event.named = named;
    event.modifiers = modifierBits;
    silk_queue_push_text(&event, keyText, keyTextLength);
}

static void keyboard_keymap(void *data, struct wl_keyboard *board, uint32_t format, int32_t fd,
                            uint32_t size) {
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || !xkb) {
        close(fd);
        return;
    }
    char *text = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (text == MAP_FAILED) return;
    struct xkb_keymap *map = xkb_keymap_new_from_string(xkb, text, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                        XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(text, size);
    if (map) use_keymap(map);
}

static void keyboard_enter(void *data, struct wl_keyboard *board, uint32_t serial,
                           struct wl_surface *surface, struct wl_array *keys) {
    WlWindow *window = find_surface(surface);
    lastSerial = serial;
    keyboardFocus = window ? window->id : 0;
}

static void keyboard_leave(void *data, struct wl_keyboard *board, uint32_t serial,
                           struct wl_surface *surface) {
    keyboardFocus = 0;
    repeatKey = 0;
    memset(pressed, 0, sizeof pressed);
}

static void keyboard_key(void *data, struct wl_keyboard *board, uint32_t serial, uint32_t time,
                         uint32_t key, uint32_t state) {
    lastSerial = serial;
    key_event(key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *board, uint32_t serial,
                               uint32_t depressed, uint32_t latched, uint32_t locked,
                               uint32_t group) {
    if (!xkbState) return;
    xkb_state_update_mask(xkbState, depressed, latched, locked, 0, 0, group);
    xkb_mod_mask_t shiftMask = 0;
    xkb_mod_index_t shiftIndex = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
    xkb_mod_index_t capsIndex = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
    if (shiftIndex != XKB_MOD_INVALID) shiftMask |= 1u << shiftIndex;
    xkb_mod_mask_t capsMask = capsIndex != XKB_MOD_INVALID ? 1u << capsIndex : 0;
    xkb_state_update_mask(shiftState, depressed & shiftMask, 0, locked & capsMask, 0, 0, group);
    uint32_t bits = current_modifiers();
    if (bits != modifierBits) {
        modifierBits = bits;
        if (keyboardFocus) push_modifiers(keyboardFocus);
    }
}

static void keyboard_repeat(void *data, struct wl_keyboard *board, int32_t rate, int32_t delay) {
    repeatRate = rate;
    repeatDelay = delay;
}

static const struct wl_keyboard_listener keyboardListener = {
    keyboard_keymap, keyboard_enter, keyboard_leave, keyboard_key, keyboard_modifiers,
    keyboard_repeat,
};

// ---------------------------------------------------------------------------------------
// Pointer and the title bar.

static const char *cursor_name(uint32_t cursor) {
    static const char *names[] = {
        "default", "text", "pointer", "crosshair", "grabbing", "grab", "not-allowed",
        "ew-resize", "ns-resize", "w-resize", "e-resize", "n-resize", "s-resize",
        "nwse-resize", "nesw-resize", "col-resize", "row-resize", "vertical-text", "copy",
        "alias", "context-menu",
    };
    return cursor < sizeof names / sizeof names[0] ? names[cursor] : "default";
}

static uint32_t cursor_shape(uint32_t cursor) {
    static const uint32_t shapes[] = {
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU,
    };
    return cursor < sizeof shapes / sizeof shapes[0] ? shapes[cursor]
                                                     : WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
}

static void apply_cursor(uint32_t cursor) {
    if (!pointer) return;
    if (cursor == 21) { // Cursor.None
        wl_pointer_set_cursor(pointer, pointerEnterSerial, NULL, 0, 0);
        return;
    }
    if (cursorShapeDevice) {
        wp_cursor_shape_device_v1_set_shape(cursorShapeDevice, pointerEnterSerial, cursor_shape(cursor));
        return;
    }
    if (!cursorTheme && shm) {
        const char *size = getenv("XCURSOR_SIZE");
        cursorTheme = wl_cursor_theme_load(getenv("XCURSOR_THEME"), size ? atoi(size) : 24, shm);
    }
    if (!cursorTheme) return;
    struct wl_cursor *found = wl_cursor_theme_get_cursor(cursorTheme, cursor_name(cursor));
    if (!found) found = wl_cursor_theme_get_cursor(cursorTheme, "left_ptr");
    if (!found || found->image_count == 0) return;
    struct wl_cursor_image *image = found->images[0];
    struct wl_buffer *buffer = wl_cursor_image_get_buffer(image);
    if (!cursorSurface) cursorSurface = wl_compositor_create_surface(compositor);
    wl_pointer_set_cursor(pointer, pointerEnterSerial, cursorSurface, (int32_t)image->hotspot_x,
                          (int32_t)image->hotspot_y);
    wl_surface_attach(cursorSurface, buffer, 0, 0);
    wl_surface_damage(cursorSurface, 0, 0, (int32_t)image->width, (int32_t)image->height);
    wl_surface_commit(cursorSurface);
}

static int title_button_at(WlWindow *window, double x) {
    double right = window->width;
    for (int button = 1; button <= 3; button++) {
        double left = right - BUTTON_WIDTH;
        if (x >= left && x < right) return (button == 2 && !window->resizable) ? 0 : button;
        right = left;
    }
    return 0;
}

static void pointer_enter(void *data, struct wl_pointer *p, uint32_t serial,
                          struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    WlWindow *window = find_surface(surface);
    pointerEnterSerial = serial;
    if (!window) return;
    pointerFocus = window->id;
    pointerOnTitle = surface == window->titleSurface;
    pointerX = wl_fixed_to_double(x);
    pointerY = wl_fixed_to_double(y);
    if (pointerOnTitle) {
        apply_cursor(0);
        return;
    }
    apply_cursor(window->cursor);
    SilkWindowEvent event = silk_event(SILK_EVENT_MOUSE_ENTERED, window->id);
    event.x = pointerX;
    event.y = pointerY;
    silk_queue_push(&event);
}

static void pointer_leave(void *data, struct wl_pointer *p, uint32_t serial,
                          struct wl_surface *surface) {
    WlWindow *window = find(pointerFocus);
    if (window && pointerOnTitle) {
        window->hoveredButton = 0;
        draw_title(window);
    } else if (window) {
        SilkWindowEvent event = silk_event(SILK_EVENT_MOUSE_EXITED, window->id);
        silk_queue_push(&event);
    }
    pointerFocus = 0;
    pointerOnTitle = false;
}

static void pointer_motion(void *data, struct wl_pointer *p, uint32_t time, wl_fixed_t x,
                           wl_fixed_t y) {
    pointerX = wl_fixed_to_double(x);
    pointerY = wl_fixed_to_double(y);
    WlWindow *window = find(pointerFocus);
    if (!window) return;
    if (pointerOnTitle) {
        int hovered = title_button_at(window, pointerX);
        if (hovered != window->hoveredButton) {
            window->hoveredButton = hovered;
            draw_title(window);
        }
        return;
    }
    SilkWindowEvent event = silk_event(SILK_EVENT_MOUSE_MOVED, window->id);
    event.x = pointerX;
    event.y = pointerY;
    event.modifiers = modifierBits;
    silk_queue_push(&event);
}

static uint32_t button_of(uint32_t button) {
    switch (button) {
    case BTN_LEFT: return SILK_BUTTON_LEFT;
    case BTN_RIGHT: return SILK_BUTTON_RIGHT;
    case BTN_MIDDLE: return SILK_BUTTON_MIDDLE;
    case BTN_SIDE:
    case BTN_BACK: return SILK_BUTTON_BACK;
    case BTN_EXTRA:
    case BTN_FORWARD: return SILK_BUTTON_FORWARD;
    default: return SILK_BUTTON_OTHER;
    }
}

static void title_button(WlWindow *window, uint32_t serial, uint32_t button, bool down) {
    if (button == BTN_RIGHT && down && seat) {
        xdg_toplevel_show_window_menu(window->toplevel, seat, serial, (int32_t)pointerX,
                                      (int32_t)pointerY - TITLE_HEIGHT);
        return;
    }
    if (button != BTN_LEFT) return;
    int hit = title_button_at(window, pointerX);
    if (down) {
        if (hit) return;
        double now = silk_now();
        bool doubleClick = now - lastClickTime < 0.4;
        lastClickTime = now;
        if (doubleClick && window->resizable) {
            if (window->maximized) xdg_toplevel_unset_maximized(window->toplevel);
            else xdg_toplevel_set_maximized(window->toplevel);
            return;
        }
        if (seat) xdg_toplevel_move(window->toplevel, seat, serial);
        return;
    }
    if (hit == 1) {
        SilkWindowEvent event = silk_event(SILK_EVENT_CLOSE_REQUESTED, window->id);
        silk_queue_push(&event);
    } else if (hit == 2) {
        if (window->maximized) xdg_toplevel_unset_maximized(window->toplevel);
        else xdg_toplevel_set_maximized(window->toplevel);
    } else if (hit == 3) {
        xdg_toplevel_set_minimized(window->toplevel);
    }
}

static void pointer_button(void *data, struct wl_pointer *p, uint32_t serial, uint32_t time,
                           uint32_t button, uint32_t state) {
    lastSerial = serial;
    WlWindow *window = find(pointerFocus);
    if (!window) return;
    bool down = state == WL_POINTER_BUTTON_STATE_PRESSED;
    if (pointerOnTitle) {
        title_button(window, serial, button, down);
        return;
    }
    if (down) {
        double now = silk_now();
        bool same = lastClickButton == button && now - lastClickTime < 0.5 &&
                    fabs(pointerX - lastClickX) <= 4 && fabs(pointerY - lastClickY) <= 4;
        clickCount = same ? clickCount + 1 : 1;
        lastClickTime = now;
        lastClickX = pointerX;
        lastClickY = pointerY;
        lastClickButton = button;
    }
    SilkWindowEvent event = silk_event(down ? SILK_EVENT_MOUSE_DOWN : SILK_EVENT_MOUSE_UP, window->id);
    event.code = button_of(button);
    event.x = pointerX;
    event.y = pointerY;
    event.modifiers = modifierBits;
    event.count = clickCount;
    silk_queue_push(&event);
}

static void pointer_axis(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis,
                         wl_fixed_t value) {
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) axisY += wl_fixed_to_double(value);
    else axisX += wl_fixed_to_double(value);
}

static void pointer_frame(void *data, struct wl_pointer *p) {
    WlWindow *window = find(pointerFocus);
    bool any = axisX != 0 || axisY != 0 || axis120X != 0 || axis120Y != 0;
    if (window && any && !pointerOnTitle) {
        SilkWindowEvent event = silk_event(SILK_EVENT_SCROLL, window->id);
        event.x = pointerX;
        event.y = pointerY;
        event.modifiers = modifierBits;
        // Wayland's positive axis scrolls down; ours shows what is above.
        if (axisDiscrete) {
            event.dx = -axis120X / 120.0;
            event.dy = -axis120Y / 120.0;
        } else {
            event.flags |= SILK_FLAG_PRECISE;
            event.dx = -axisX;
            event.dy = -axisY;
        }
        silk_queue_push(&event);
    }
    axisX = axisY = 0;
    axis120X = axis120Y = 0;
    axisDiscrete = false;
}

static void pointer_axis_source(void *data, struct wl_pointer *p, uint32_t source) {}
static void pointer_axis_stop(void *data, struct wl_pointer *p, uint32_t time, uint32_t axis) {}
static void pointer_axis_discrete(void *data, struct wl_pointer *p, uint32_t axis, int32_t discrete) {
    axisDiscrete = true;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) axis120Y += discrete * 120;
    else axis120X += discrete * 120;
}
static void pointer_axis_value120(void *data, struct wl_pointer *p, uint32_t axis, int32_t value) {
    axisDiscrete = true;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) axis120Y += value;
    else axis120X += value;
}
static void pointer_axis_direction(void *data, struct wl_pointer *p, uint32_t axis, uint32_t direction) {}

static const struct wl_pointer_listener pointerListener = {
    pointer_enter, pointer_leave, pointer_motion, pointer_button, pointer_axis, pointer_frame,
    pointer_axis_source, pointer_axis_stop, pointer_axis_discrete, pointer_axis_value120,
    pointer_axis_direction,
};

// ---------------------------------------------------------------------------------------
// Text input (zwp_text_input_v3).

static void text_enter(void *data, struct zwp_text_input_v3 *input, struct wl_surface *surface) {
    WlWindow *window = find_surface(surface);
    textFocus = window ? window->id : 0;
    if (window && window->imeEnabled) {
        zwp_text_input_v3_enable(input);
        zwp_text_input_v3_set_cursor_rectangle(input, (int32_t)window->imeX, (int32_t)window->imeY,
                                               (int32_t)window->imeWidth, (int32_t)window->imeHeight);
        zwp_text_input_v3_commit(input);
        textInputActive = true;
    }
}

static void text_leave(void *data, struct zwp_text_input_v3 *input, struct wl_surface *surface) {
    if (textInputActive) {
        zwp_text_input_v3_disable(input);
        zwp_text_input_v3_commit(input);
        textInputActive = false;
    }
    textFocus = 0;
}

static void text_preedit(void *data, struct zwp_text_input_v3 *input, const char *text,
                         int32_t begin, int32_t end) {
    free(pendingPreedit);
    pendingPreedit = strdup(text ? text : "");
    pendingPreeditBegin = begin;
    pendingPreeditEnd = end;
}

static void text_commit(void *data, struct zwp_text_input_v3 *input, const char *text) {
    free(pendingCommit);
    pendingCommit = text ? strdup(text) : NULL;
}

static void text_delete(void *data, struct zwp_text_input_v3 *input, uint32_t before,
                        uint32_t after) {}

static void text_done(void *data, struct zwp_text_input_v3 *input, uint32_t serial) {
    uint32_t window = textFocus;
    if (!window) return;
    // The protocol's order: replace the preedit with the commit, then show the new preedit.
    char *preedit = pendingPreedit ? pendingPreedit : strdup("");
    SilkWindowEvent event = silk_event(SILK_EVENT_IME_PREEDIT, window);
    size_t length = strlen(preedit);
    event.start = pendingPreeditBegin < 0 ? length : (uint64_t)pendingPreeditBegin;
    event.end = pendingPreeditEnd < 0 ? length : (uint64_t)pendingPreeditEnd;
    if (pendingCommit) {
        SilkWindowEvent clear = silk_event(SILK_EVENT_IME_PREEDIT, window);
        silk_queue_push_text(&clear, NULL, 0);
        push_text(window, (const uint8_t *)pendingCommit, strlen(pendingCommit));
    }
    if (length || !pendingCommit) silk_queue_push_text(&event, (const uint8_t *)preedit, length);
    free(preedit);
    free(pendingCommit);
    pendingPreedit = NULL;
    pendingCommit = NULL;
    pendingPreeditBegin = pendingPreeditEnd = 0;
}

static const struct zwp_text_input_v3_listener textInputListener = {
    text_enter, text_leave, text_preedit, text_commit, text_delete, text_done,
};

// ---------------------------------------------------------------------------------------
// Clipboard.

static void source_target(void *data, struct wl_data_source *source, const char *mime) {}

static void source_send(void *data, struct wl_data_source *source, const char *mime, int32_t fd) {
    size_t written = 0;
    while (written < ownLength) {
        ssize_t n = write(fd, ownText + written, ownLength - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += (size_t)n;
    }
    close(fd);
}

static void source_cancelled(void *data, struct wl_data_source *source) {
    if (source == ownSource) ownSource = NULL;
    wl_data_source_destroy(source);
}

static void source_dnd_drop(void *data, struct wl_data_source *source) {}
static void source_dnd_finished(void *data, struct wl_data_source *source) {}
static void source_action(void *data, struct wl_data_source *source, uint32_t action) {}

static const struct wl_data_source_listener sourceListener = {
    source_target, source_send, source_cancelled, source_dnd_drop, source_dnd_finished,
    source_action,
};

static void offer_offer(void *data, struct wl_data_offer *offer, const char *mime) {
    if (strcmp(mime, "text/plain;charset=utf-8") == 0 || strcmp(mime, "UTF8_STRING") == 0 ||
        strcmp(mime, "text/plain") == 0)
        *(bool *)wl_data_offer_get_user_data(offer) = true;
}
static void offer_source_actions(void *data, struct wl_data_offer *offer, uint32_t actions) {}
static void offer_action(void *data, struct wl_data_offer *offer, uint32_t action) {}

static const struct wl_data_offer_listener offerListener = {
    offer_offer, offer_source_actions, offer_action,
};

static bool offerFlags[64];
static int offerFlagNext;

static void device_data_offer(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    bool *flag = &offerFlags[offerFlagNext++ % 64];
    *flag = false;
    wl_data_offer_add_listener(offer, &offerListener, flag);
}

static void device_enter(void *data, struct wl_data_device *device, uint32_t serial,
                         struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
                         struct wl_data_offer *offer) {
    if (offer) wl_data_offer_destroy(offer);
}
static void device_leave(void *data, struct wl_data_device *device) {}
static void device_motion(void *data, struct wl_data_device *device, uint32_t time, wl_fixed_t x,
                          wl_fixed_t y) {}
static void device_drop(void *data, struct wl_data_device *device) {}

static void device_selection(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    if (selectionOffer) wl_data_offer_destroy(selectionOffer);
    selectionOffer = offer;
    selectionOfferHasText = offer && *(bool *)wl_data_offer_get_user_data(offer);
}

static const struct wl_data_device_listener deviceListener = {
    device_data_offer, device_enter, device_leave, device_motion, device_drop, device_selection,
};

int64_t silk_wl_clipboard_read(uint8_t *destination, size_t capacity) {
    if (ownSource) {
        if (destination && capacity) memcpy(destination, ownText, ownLength < capacity ? ownLength : capacity);
        return (int64_t)ownLength;
    }
    if (destination && readLength >= 0) {
        size_t length = (size_t)readLength;
        if (capacity) memcpy(destination, readText, length < capacity ? length : capacity);
        return readLength;
    }
    free(readText);
    readText = NULL;
    readLength = -1;
    if (!selectionOffer || !selectionOfferHasText) return -1;
    int pipes[2];
    if (pipe2(pipes, O_CLOEXEC) != 0) return -1;
    wl_data_offer_receive(selectionOffer, "text/plain;charset=utf-8", pipes[1]);
    close(pipes[1]);
    wl_display_flush(display);
    size_t length = 0, size = 0;
    uint8_t *text = NULL;
    double deadline = silk_now() + 1.0;
    for (;;) {
        double now = silk_now();
        if (now >= deadline) break;
        struct pollfd fd = {pipes[0], POLLIN, 0};
        if (poll(&fd, 1, (int)ceil((deadline - now) * 1000.0)) <= 0) break;
        if (length + 4096 > size) {
            size = (length + 4096) * 2;
            uint8_t *grown = realloc(text, size);
            if (!grown) abort();
            text = grown;
        }
        ssize_t n = read(pipes[0], text + length, 4096);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        length += (size_t)n;
    }
    close(pipes[0]);
    readText = text ? text : malloc(1);
    readLength = (int64_t)length;
    if (destination && capacity) memcpy(destination, readText, length < capacity ? length : capacity);
    return readLength;
}

void silk_wl_clipboard_write(const uint8_t *text, size_t length) {
    free(ownText);
    ownText = malloc(length ? length : 1);
    if (!ownText) abort();
    if (length) memcpy(ownText, text, length);
    ownLength = length;
    if (ownSource) {
        wl_data_source_destroy(ownSource);
        ownSource = NULL;
    }
    if (!dataDeviceManager) {
        return;
    }
    ownSource = wl_data_device_manager_create_data_source(dataDeviceManager);
    wl_data_source_add_listener(ownSource, &sourceListener, NULL);
    wl_data_source_offer(ownSource, "text/plain;charset=utf-8");
    wl_data_source_offer(ownSource, "UTF8_STRING");
    wl_data_source_offer(ownSource, "text/plain");
    if (dataDevice) wl_data_device_set_selection(dataDevice, ownSource, lastSerial);
    wl_display_flush(display);
}

// ---------------------------------------------------------------------------------------
// Seat and registry.

static void seat_capabilities(void *data, struct wl_seat *s, uint32_t capabilities) {
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
        keyboard = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(keyboard, &keyboardListener, NULL);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
        wl_keyboard_release(keyboard);
        keyboard = NULL;
    }
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
        pointer = wl_seat_get_pointer(s);
        wl_pointer_add_listener(pointer, &pointerListener, NULL);
        if (cursorShapeManager)
            cursorShapeDevice = wp_cursor_shape_manager_v1_get_pointer(cursorShapeManager, pointer);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && pointer) {
        wl_pointer_release(pointer);
        pointer = NULL;
    }
}

static void seat_name(void *data, struct wl_seat *s, const char *name) {}

static const struct wl_seat_listener seatListener = {seat_capabilities, seat_name};

static uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

static void registry_global(void *data, struct wl_registry *r, uint32_t name,
                            const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositorVersion = min_u32(version, 6);
        compositor = wl_registry_bind(r, name, &wl_compositor_interface, compositorVersion);
    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        subcompositor = wl_registry_bind(r, name, &wl_subcompositor_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        wmBase = wl_registry_bind(r, name, &xdg_wm_base_interface, min_u32(version, 5));
        xdg_wm_base_add_listener(wmBase, &wmBaseListener, NULL);
    } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        decorationManager = wl_registry_bind(r, name, &zxdg_decoration_manager_v1_interface, 1);
    } else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0) {
        fractionalManager = wl_registry_bind(r, name, &wp_fractional_scale_manager_v1_interface, 1);
    } else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
        viewporter = wl_registry_bind(r, name, &wp_viewporter_interface, 1);
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
        cursorShapeManager = wl_registry_bind(r, name, &wp_cursor_shape_manager_v1_interface, 1);
    } else if (strcmp(interface, zwp_text_input_manager_v3_interface.name) == 0) {
        textInputManager = wl_registry_bind(r, name, &zwp_text_input_manager_v3_interface, 1);
    } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        dataDeviceManager = wl_registry_bind(r, name, &wl_data_device_manager_interface,
                                             min_u32(version, 3));
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !seat) {
        seat = wl_registry_bind(r, name, &wl_seat_interface, min_u32(version, 8));
        wl_seat_add_listener(seat, &seatListener, NULL);
    } else if (strcmp(interface, wl_output_interface.name) == 0 && outputCount < 16) {
        WlOutput *output = &outputs[outputCount++];
        output->name = name;
        output->scale = 1;
        output->output = wl_registry_bind(r, name, &wl_output_interface, min_u32(version, 4));
        wl_output_add_listener(output->output, &outputListener, output);
    }
}

static void registry_remove(void *data, struct wl_registry *r, uint32_t name) {}

static const struct wl_registry_listener registryListener = {registry_global, registry_remove};

bool silk_wl_start(void) {
    display = wl_display_connect(NULL);
    if (!display) return false;
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, NULL);
    wl_display_roundtrip(display);
    if (!compositor || !wmBase) {
        wl_display_disconnect(display);
        display = NULL;
        return false;
    }
    wl_display_roundtrip(display); // seat capabilities and outputs
    if (seat && dataDeviceManager) {
        dataDevice = wl_data_device_manager_get_data_device(dataDeviceManager, seat);
        wl_data_device_add_listener(dataDevice, &deviceListener, NULL);
    }
    if (seat && textInputManager) {
        textInput = zwp_text_input_manager_v3_get_text_input(textInputManager, seat);
        zwp_text_input_v3_add_listener(textInput, &textInputListener, NULL);
    }
    const char *current = setlocale(LC_CTYPE, NULL);
    if (!current || strcmp(current, "C") == 0) setlocale(LC_CTYPE, "");
    xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    const char *locale = getenv("LC_ALL");
    if (!locale || !*locale) locale = getenv("LC_CTYPE");
    if (!locale || !*locale) locale = getenv("LANG");
    if (!locale || !*locale) locale = "C";
    if (xkb) {
        composeTable = xkb_compose_table_new_from_locale(xkb, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (composeTable) composeState = xkb_compose_state_new(composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
    }
    const char *hz = getenv("SILK_WINDOW_REFRESH_HZ");
    if (hz && atof(hz) > 0) refreshInterval = 1.0 / atof(hz);
    return true;
}

// ---------------------------------------------------------------------------------------
// Windows.

static void create_role(WlWindow *window) {
    window->xdgSurface = xdg_wm_base_get_xdg_surface(wmBase, window->surface);
    xdg_surface_add_listener(window->xdgSurface, &xdgSurfaceListener, window);
    window->toplevel = xdg_surface_get_toplevel(window->xdgSurface);
    xdg_toplevel_add_listener(window->toplevel, &toplevelListener, window);
    xdg_toplevel_set_app_id(window->toplevel, "silk");
    if (window->title) xdg_toplevel_set_title(window->toplevel, window->title);
    double title = window->csd ? TITLE_HEIGHT : 0;
    xdg_toplevel_set_min_size(window->toplevel, (int32_t)lround(window->minWidth),
                              (int32_t)lround(window->minHeight + title));
    if (!window->resizable) {
        xdg_toplevel_set_max_size(window->toplevel, (int32_t)lround(window->width),
                                  (int32_t)lround(window->height + title));
        xdg_toplevel_set_min_size(window->toplevel, (int32_t)lround(window->width),
                                  (int32_t)lround(window->height + title));
    }
    if (decorationManager) {
        window->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(decorationManager,
                                                                                 window->toplevel);
        zxdg_toplevel_decoration_v1_add_listener(window->decoration, &decorationListener, window);
        zxdg_toplevel_decoration_v1_set_mode(window->decoration,
                                             window->titleHidden
                                                 ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
                                                 : ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
    wl_surface_commit(window->surface);
    // Wait for the first configure so the size and decorations are known on return.
    for (int i = 0; i < 50 && !window->configured; i++)
        if (wl_display_roundtrip(display) < 0) break;
}

static void destroy_role(WlWindow *window) {
    if (window->callback) {
        wl_callback_destroy(window->callback);
        window->callback = NULL;
    }
    if (window->decoration) zxdg_toplevel_decoration_v1_destroy(window->decoration);
    if (window->toplevel) xdg_toplevel_destroy(window->toplevel);
    if (window->xdgSurface) xdg_surface_destroy(window->xdgSurface);
    window->decoration = NULL;
    window->toplevel = NULL;
    window->xdgSurface = NULL;
    window->configured = false;
    wl_surface_attach(window->surface, NULL, 0, 0);
    wl_surface_commit(window->surface);
}

uint32_t silk_wl_open(const uint8_t *title, size_t titleLength, double width, double height,
                      double minWidth, double minHeight, uint32_t flags) {
    if (windowCount == windowCapacity) {
        size_t capacity = windowCapacity ? windowCapacity * 2 : 8;
        WlWindow **grown = realloc(windows, capacity * sizeof *grown);
        if (!grown) return 0;
        windows = grown;
        windowCapacity = capacity;
    }
    WlWindow *window = malloc(sizeof *window);
    if (!window) return 0;
    windows[windowCount++] = window;
    memset(window, 0, sizeof *window);
    window->id = nextWindow++;
    window->open = true;
    window->width = width;
    window->height = height;
    window->minWidth = minWidth;
    window->minHeight = minHeight;
    window->resizable = (flags & SILK_WINDOW_RESIZABLE) != 0;
    window->titleHidden = (flags & SILK_WINDOW_TITLE_HIDDEN) != 0;
    // Without xdg-decoration the client draws its own title bar.
    window->csd = !decorationManager && !window->titleHidden;
    window->title = silk_strndup(title, titleLength);
    window->scale = 1.0;
    window->surface = wl_compositor_create_surface(compositor);
    wl_surface_add_listener(window->surface, &surfaceListener, window);
    if (fractionalManager && viewporter) {
        window->fractional = wp_fractional_scale_manager_v1_get_fractional_scale(fractionalManager,
                                                                                window->surface);
        wp_fractional_scale_v1_add_listener(window->fractional, &fractionalListener, window);
        window->viewport = wp_viewporter_get_viewport(viewporter, window->surface);
    }
    if (subcompositor && shm) {
        window->titleSurface = wl_compositor_create_surface(compositor);
        window->titleSubsurface = wl_subcompositor_get_subsurface(subcompositor, window->titleSurface,
                                                                  window->surface);
        wl_subsurface_set_desync(window->titleSubsurface);
    } else {
        window->csd = false;
    }
    window->scale = compute_scale(window);
    if (flags & SILK_WINDOW_VISIBLE) silk_wl_set_visible(window->id, true);
    wl_display_flush(display);
    return window->id;
}

void silk_wl_close(uint32_t id) {
    WlWindow *window = find(id);
    if (!window) return;
    if (keyboardFocus == id) keyboardFocus = 0;
    if (pointerFocus == id) pointerFocus = 0;
    if (textFocus == id) textFocus = 0;
    destroy_role(window);
    if (window->titleSubsurface) wl_subsurface_destroy(window->titleSubsurface);
    if (window->titleSurface) wl_surface_destroy(window->titleSurface);
    if (window->viewport) wp_viewport_destroy(window->viewport);
    if (window->fractional) wp_fractional_scale_v1_destroy(window->fractional);
    wl_surface_destroy(window->surface);
    free(window->title);
    window->title = NULL;
    window->open = false;
    wl_display_flush(display);
}

// ---------------------------------------------------------------------------------------
// The loop.

void silk_wl_dispatch(void) { wl_display_dispatch_pending(display); }

int silk_wl_fd(void) { return wl_display_get_fd(display); }

bool silk_wl_prepare_wait(void) {
    if (wl_display_prepare_read(display) != 0) {
        wl_display_dispatch_pending(display);
        return false;
    }
    wl_display_flush(display);
    return true;
}

void silk_wl_cancel_wait(void) { wl_display_cancel_read(display); }

void silk_wl_finish_wait(bool readable) {
    if (readable) wl_display_read_events(display);
    else wl_display_cancel_read(display);
}

static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    WlWindow *window = data;
    wl_callback_destroy(callback);
    if (window->callback == callback) window->callback = NULL;
    window->callbackEverFired = true;
    if (!window->open || !window->frame.pending) return;
    window->frame.pending = false;
    SilkWindowEvent event = silk_event(SILK_EVENT_REQUEST_FRAME, window->id);
    event.value = silk_now() + refreshInterval;
    silk_queue_push(&event);
}

static const struct wl_callback_listener frameListener = {frame_done};

double silk_wl_frames(double now) {
    double next = INFINITY;
    for (size_t i = 0; i < windowCount; i++) {
        WlWindow *window = windows[i];
        if (!window->open || !window->frame.pending || !window->visible || !window->configured) continue;
        bool due = window->frame.forced || (!window->callbackEverFired && now >= window->due);
        if (!due) {
            if (!window->callbackEverFired && window->due < next) next = window->due;
            continue;
        }
        window->frame.pending = window->frame.forced = false;
        SilkWindowEvent event = silk_event(SILK_EVENT_REQUEST_FRAME, window->id);
        event.value = now + refreshInterval;
        silk_queue_push(&event);
    }
    if (repeatKey && keyboardFocus) {
        if (now >= repeatNext) {
            key_down(repeatKey, true);
            repeatNext = now + 1.0 / repeatRate;
        }
        if (repeatNext < next) next = repeatNext;
    }
    return next;
}

void silk_wl_request_frame(uint32_t id) {
    WlWindow *window = find(id);
    if (!window || window->frame.pending) return;
    window->frame.pending = true;
    window->due = silk_now() + refreshInterval;
    if (!window->callback && window->configured) {
        window->callback = wl_surface_frame(window->surface);
        wl_callback_add_listener(window->callback, &frameListener, window);
        wl_surface_commit(window->surface);
    }
}

void silk_wl_content_size(uint32_t id, double *width, double *height) {
    WlWindow *window = find(id);
    if (!window) return;
    *width = window->width;
    *height = window->height;
}

double silk_wl_scale_factor(uint32_t id) {
    WlWindow *window = find(id);
    return window ? window->scale : 1.0;
}

uint32_t silk_wl_raw_surface(uint32_t id, void **first, void **second, uint64_t *xid) {
    WlWindow *window = find(id);
    if (!window) return SILK_SURFACE_NONE;
    *first = display;
    *second = window->surface;
    return SILK_SURFACE_WAYLAND;
}

void silk_wl_set_title(uint32_t id, const uint8_t *title, size_t length) {
    WlWindow *window = find(id);
    if (!window) return;
    free(window->title);
    window->title = silk_strndup(title, length);
    if (window->toplevel) xdg_toplevel_set_title(window->toplevel, window->title);
}

void silk_wl_set_content_size(uint32_t id, double width, double height) {
    WlWindow *window = find(id);
    if (!window) return;
    window->width = width;
    window->height = height;
    if (!window->configured) return;
    if (!window->resizable) {
        double title = window->csd ? TITLE_HEIGHT : 0;
        xdg_toplevel_set_max_size(window->toplevel, (int32_t)lround(width), (int32_t)lround(height + title));
        xdg_toplevel_set_min_size(window->toplevel, (int32_t)lround(width), (int32_t)lround(height + title));
    }
    apply_scale(window);
    set_geometry(window);
    draw_title(window);
    wl_surface_commit(window->surface);
    push_resized(window);
}

void silk_wl_set_min_content_size(uint32_t id, double width, double height) {
    WlWindow *window = find(id);
    if (!window) return;
    window->minWidth = width;
    window->minHeight = height;
    if (window->toplevel && window->resizable) {
        double title = window->csd ? TITLE_HEIGHT : 0;
        xdg_toplevel_set_min_size(window->toplevel, (int32_t)lround(width), (int32_t)lround(height + title));
        wl_surface_commit(window->surface);
    }
}

void silk_wl_set_visible(uint32_t id, bool visible) {
    WlWindow *window = find(id);
    if (!window || window->visible == visible) return;
    window->visible = visible;
    // Wayland has no hide: a hidden window gives up its role and gets a new one when shown.
    if (visible) {
        create_role(window);
    } else {
        destroy_role(window);
        window->frame.pending = false;
    }
}

void silk_wl_focus(uint32_t id) {
    // Wayland lets only the compositor move focus; xdg-activation would need a token from
    // an input event. Nothing to do.
    (void)id;
}

void silk_wl_minimize(uint32_t id) {
    WlWindow *window = find(id);
    if (window && window->toplevel) xdg_toplevel_set_minimized(window->toplevel);
}

void silk_wl_toggle_full_screen(uint32_t id) {
    WlWindow *window = find(id);
    if (!window || !window->toplevel) return;
    if (window->fullScreen) xdg_toplevel_unset_fullscreen(window->toplevel);
    else xdg_toplevel_set_fullscreen(window->toplevel, NULL);
}

void silk_wl_start_window_drag(uint32_t id) {
    WlWindow *window = find(id);
    if (window && window->toplevel && seat) xdg_toplevel_move(window->toplevel, seat, lastSerial);
}

void silk_wl_set_cursor(uint32_t id, uint32_t cursor) {
    WlWindow *window = find(id);
    if (!window) return;
    window->cursor = cursor;
    if (pointerFocus == id && !pointerOnTitle) apply_cursor(cursor);
}

void silk_wl_set_ime(uint32_t id, bool enabled, double x, double y, double width, double height) {
    WlWindow *window = find(id);
    if (!window) return;
    window->imeEnabled = enabled;
    window->imeX = x;
    window->imeY = y;
    window->imeWidth = width;
    window->imeHeight = height;
    if (!textInput || textFocus != id) return;
    if (enabled) {
        if (!textInputActive) zwp_text_input_v3_enable(textInput);
        zwp_text_input_v3_set_content_type(textInput, ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE,
                                           ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
        zwp_text_input_v3_set_cursor_rectangle(textInput, (int32_t)lround(x), (int32_t)lround(y),
                                               (int32_t)lround(width), (int32_t)lround(height));
        textInputActive = true;
    } else if (textInputActive) {
        zwp_text_input_v3_disable(textInput);
        textInputActive = false;
    }
    zwp_text_input_v3_commit(textInput);
}

// ---------------------------------------------------------------------------------------
// Test hooks: Wayland clients cannot inject input, so these feed the same handlers the
// compositor's events reach.

uint8_t silk_wl_test_key(uint32_t id, uint32_t code, const uint8_t *text, size_t length,
                         uint32_t modifiers) {
    WlWindow *window = find(id);
    uint32_t evdev = silk_evdev_from_code(code);
    if (!window || !evdev) return 0;
    ensure_keymap();
    if (!xkbState) return 0;
    uint32_t savedFocus = keyboardFocus, savedModifiers = modifierBits;
    keyboardFocus = id;
    xkb_mod_mask_t mask = 0;
    struct {
        uint32_t bit;
        const char *name;
    } names[] = {{SILK_MOD_SHIFT, XKB_MOD_NAME_SHIFT}, {SILK_MOD_CONTROL, XKB_MOD_NAME_CTRL},
                 {SILK_MOD_ALT, XKB_MOD_NAME_ALT}, {SILK_MOD_META, XKB_MOD_NAME_LOGO}};
    for (size_t i = 0; i < 4; i++)
        if (modifiers & names[i].bit) {
            xkb_mod_index_t index = xkb_keymap_mod_get_index(keymap, names[i].name);
            if (index != XKB_MOD_INVALID) mask |= 1u << index;
        }
    keyboard_modifiers(NULL, NULL, 0, mask, 0, 0, 0);
    key_event(evdev, true);
    repeatKey = 0;
    key_event(evdev, false);
    keyboard_modifiers(NULL, NULL, 0, 0, 0, 0, 0);
    // Restoring the modifiers queued ModifiersChanged twice; the test cares about keys.
    keyboardFocus = savedFocus ? savedFocus : id;
    modifierBits = savedModifiers;
    return 1;
}

uint8_t silk_wl_test_mouse(uint32_t id, uint32_t kind, uint32_t button, double x, double y,
                           double dx, double dy) {
    WlWindow *window = find(id);
    if (!window) return 0;
    if (pointerFocus != id || pointerOnTitle)
        pointer_enter(NULL, NULL, 0, window->surface, wl_fixed_from_double(x), wl_fixed_from_double(y));
    static const uint32_t buttons[] = {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA, BTN_TASK};
    uint32_t which = button < 6 ? buttons[button] : BTN_LEFT;
    if (kind == 0) {
        pointer_motion(NULL, NULL, 0, wl_fixed_from_double(x), wl_fixed_from_double(y));
    } else if (kind == 1 || kind == 2) {
        pointer_button(NULL, NULL, 0, 0, which, kind == 1 ? WL_POINTER_BUTTON_STATE_PRESSED
                                                         : WL_POINTER_BUTTON_STATE_RELEASED);
    } else {
        pointer_axis_value120(NULL, NULL, WL_POINTER_AXIS_VERTICAL_SCROLL, (int32_t)lround(-dy * 120));
        if (dx != 0) pointer_axis_value120(NULL, NULL, WL_POINTER_AXIS_HORIZONTAL_SCROLL, (int32_t)lround(-dx * 120));
        pointer_frame(NULL, NULL);
    }
    return 1;
}

uint8_t silk_wl_test_preedit(uint32_t id, const uint8_t *text, size_t length, uint64_t start,
                             uint64_t end) {
    if (!find(id)) return 0;
    uint32_t saved = textFocus;
    textFocus = id;
    char *copy = silk_strndup(text, length);
    text_preedit(NULL, NULL, copy, (int32_t)start, (int32_t)end);
    free(copy);
    text_done(NULL, NULL, 0);
    textFocus = saved;
    return 1;
}

uint8_t silk_wl_test_commit(uint32_t id, const uint8_t *text, size_t length) {
    if (!find(id)) return 0;
    uint32_t saved = textFocus;
    textFocus = id;
    char *copy = silk_strndup(text, length);
    text_preedit(NULL, NULL, "", 0, 0);
    text_commit(NULL, NULL, copy);
    free(copy);
    text_done(NULL, NULL, 0);
    textFocus = saved;
    return 1;
}

uint8_t silk_wl_test_resize(uint32_t id, double width, double height) {
    WlWindow *window = find(id);
    if (!window || !window->configured) return 0;
    // As the compositor's configure for an interactive resize would.
    struct wl_array states;
    wl_array_init(&states);
    toplevel_configure(window, window->toplevel, (int32_t)lround(width),
                       (int32_t)lround(height + title_height(window)), &states);
    wl_array_release(&states);
    window->pendingWidth = width;
    window->pendingHeight = height + title_height(window);
    double title = title_height(window);
    double w = window->pendingWidth, h = window->pendingHeight - title;
    bool changed = fabs(w - window->width) > 1e-9 || fabs(h - window->height) > 1e-9;
    window->width = w;
    window->height = h;
    apply_scale(window);
    set_geometry(window);
    draw_title(window);
    wl_surface_commit(window->surface);
    if (changed) push_resized(window);
    return 1;
}

void silk_wl_test_ime_rect(uint32_t id, double *x, double *y, double *width, double *height) {
    WlWindow *window = find(id);
    if (!window) return;
    *x = window->imeX;
    *y = window->imeY;
    *width = window->imeWidth;
    *height = window->imeHeight;
}
