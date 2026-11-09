// The X11 half of the Linux backend: Xlib windows, XIM for input methods (on-the-spot
// preedit callbacks when the input method offers them), the CLIPBOARD selection, and
// frames paced by a timer at the display's refresh interval. A surface presented with FIFO
// then blocks on the real vertical blank.
#define _GNU_SOURCE
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include "linux.h"

typedef struct X11Window {
    uint32_t id;
    Window xid;
    Colormap colormap;
    XIC ic;
    bool open;
    bool visible;   // what the app asked for
    bool mapped;    // what the server reports
    bool focused;
    bool fullScreen;
    bool icFocused;
    int pixelWidth, pixelHeight;
    int x, y;
    double minWidth, minHeight; // points
    bool resizable;
    SilkFrameState frame;
    double due; // when a requested frame is delivered: the next refresh tick
    uint32_t cursor;
    double imeX, imeY, imeWidth, imeHeight; // points
    bool imeEnabled;
    uint32_t preedit[256];
    size_t preeditLength;
    size_t preeditCaret;
    // Click counting.
    double lastClickTime;
    int lastClickX, lastClickY;
    unsigned lastClickButton;
    uint32_t clickCount;
} X11Window;

static Display *display;
static int screen;
static Window root;
static Window helper; // owns selections
static XIM im;
static XIMStyle imStyle;
static double scale = 1.0;
static double refreshInterval = 1.0 / 60.0;
static double frameEpoch;
static X11Window *windows;
static size_t windowCount, windowCapacity;
static uint32_t nextWindow = 1;
static uint8_t pressed[32];
static Cursor cursors[32];
static Cursor blankCursor;
static uint8_t *clipboardText;
static size_t clipboardLength;
static bool clipboardOwned;
static uint8_t *clipboardRead;
static int64_t clipboardReadLength = -1;

static Atom WM_PROTOCOLS, WM_DELETE_WINDOW, NET_WM_NAME, UTF8_STRING, NET_WM_STATE,
    NET_WM_STATE_FULLSCREEN, NET_WM_MOVERESIZE, NET_ACTIVE_WINDOW, MOTIF_WM_HINTS, CLIPBOARD,
    TARGETS, SILK_SELECTION, NET_WM_PID, RESOURCE_MANAGER, TEXT_ATOM;

static X11Window *find(uint32_t id) {
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i].id == id && windows[i].open) return &windows[i];
    return NULL;
}

static X11Window *find_xid(Window xid) {
    for (size_t i = 0; i < windowCount; i++)
        if (windows[i].xid == xid && windows[i].open) return &windows[i];
    return NULL;
}

static double read_scale(void) {
    const char *forced = getenv("SILK_WINDOW_SCALE");
    if (forced) {
        double value = atof(forced);
        if (value > 0) return value;
    }
    double dpi = 96.0;
    char *resources = XResourceManagerString(display);
    if (resources) {
        XrmDatabase database = XrmGetStringDatabase(resources);
        if (database) {
            char *type = NULL;
            XrmValue value;
            if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
                double parsed = atof(value.addr);
                if (parsed > 0) dpi = parsed;
            }
            XrmDestroyDatabase(database);
        }
    }
    double factor = dpi / 96.0;
    return factor < 0.5 ? 1.0 : factor;
}

static uint32_t modifiers_from_state(unsigned state) {
    uint32_t bits = 0;
    if (state & ShiftMask) bits |= SILK_MOD_SHIFT;
    if (state & ControlMask) bits |= SILK_MOD_CONTROL;
    if (state & Mod1Mask) bits |= SILK_MOD_ALT;
    if (state & Mod4Mask) bits |= SILK_MOD_META;
    if (state & LockMask) bits |= SILK_MOD_CAPS_LOCK;
    return bits;
}

static unsigned state_from_modifiers(uint32_t bits) {
    unsigned state = 0;
    if (bits & SILK_MOD_SHIFT) state |= ShiftMask;
    if (bits & SILK_MOD_CONTROL) state |= ControlMask;
    if (bits & SILK_MOD_ALT) state |= Mod1Mask;
    if (bits & SILK_MOD_META) state |= Mod4Mask;
    if (bits & SILK_MOD_CAPS_LOCK) state |= LockMask;
    return state;
}

static void force_frame(X11Window *window) {
    window->frame.pending = true;
    window->frame.forced = true;
}

static void push_resized(X11Window *window) {
    SilkWindowEvent event = silk_event(SILK_EVENT_RESIZED, window->id);
    event.x = window->pixelWidth / scale;
    event.y = window->pixelHeight / scale;
    event.value = scale;
    silk_queue_push(&event);
    force_frame(window);
}

static void apply_size_hints(X11Window *window) {
    XSizeHints *hints = XAllocSizeHints();
    if (!hints) return;
    hints->flags = PMinSize;
    hints->min_width = (int)ceil(window->minWidth * scale);
    hints->min_height = (int)ceil(window->minHeight * scale);
    if (hints->min_width < 1) hints->min_width = 1;
    if (hints->min_height < 1) hints->min_height = 1;
    if (!window->resizable) {
        hints->flags |= PMaxSize;
        hints->min_width = hints->max_width = window->pixelWidth;
        hints->min_height = hints->max_height = window->pixelHeight;
    }
    XSetWMNormalHints(display, window->xid, hints);
    XFree(hints);
}

// ---------------------------------------------------------------------------------------
// Input methods.

static void push_preedit(X11Window *window) {
    uint8_t text[1024];
    size_t length = 0;
    size_t caretBytes = 0;
    for (size_t i = 0; i < window->preeditLength; i++) {
        if (i == window->preeditCaret) caretBytes = length;
        if (length + 4 > sizeof text) break;
        length += silk_utf8_encode(window->preedit[i], text + length);
    }
    if (window->preeditCaret >= window->preeditLength) caretBytes = length;
    SilkWindowEvent event = silk_event(SILK_EVENT_IME_PREEDIT, window->id);
    event.start = event.end = caretBytes;
    silk_queue_push_text(&event, text, length);
}

static void push_commit(X11Window *window, const uint8_t *text, size_t length) {
    if (length == 0 || silk_utf8_is_control(text, length)) return;
    SilkWindowEvent event = silk_event(SILK_EVENT_TEXT_INPUT, window->id);
    silk_queue_push_text(&event, text, length);
}

static size_t decode_utf8(const char *text, size_t length, uint32_t *out, size_t capacity) {
    size_t count = 0;
    size_t i = 0;
    while (i < length && count < capacity) {
        unsigned char c = (unsigned char)text[i];
        size_t size = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
        if (i + size > length) break;
        out[count++] = silk_utf8_first((const uint8_t *)text + i, size);
        i += size;
    }
    return count;
}

static int preedit_start(XIC ic, XPointer client, XPointer data) {
    (void)ic;
    (void)data;
    X11Window *window = find((uint32_t)(uintptr_t)client);
    if (window) window->preeditLength = window->preeditCaret = 0;
    return -1; // no length limit
}

static void preedit_done(XIC ic, XPointer client, XPointer data) {
    (void)ic;
    (void)data;
    X11Window *window = find((uint32_t)(uintptr_t)client);
    if (!window) return;
    window->preeditLength = window->preeditCaret = 0;
    push_preedit(window);
}

static void preedit_draw(XIC ic, XPointer client, XIMPreeditDrawCallbackStruct *draw) {
    (void)ic;
    X11Window *window = find((uint32_t)(uintptr_t)client);
    if (!window || !draw) return;
    uint32_t inserted[256];
    size_t insertedLength = 0;
    if (draw->text) {
        if (draw->text->encoding_is_wchar) {
            const wchar_t *wide = draw->text->string.wide_char;
            for (size_t i = 0; wide && i < draw->text->length && insertedLength < 256; i++)
                inserted[insertedLength++] = (uint32_t)wide[i];
        } else if (draw->text->string.multi_byte) {
            const char *bytes = draw->text->string.multi_byte;
            insertedLength = decode_utf8(bytes, strlen(bytes), inserted, 256);
        }
    }
    size_t first = draw->chg_first < 0 ? 0 : (size_t)draw->chg_first;
    size_t removed = draw->chg_length < 0 ? 0 : (size_t)draw->chg_length;
    if (first > window->preeditLength) first = window->preeditLength;
    if (first + removed > window->preeditLength) removed = window->preeditLength - first;
    size_t tail = window->preeditLength - first - removed;
    if (first + insertedLength + tail > 256) return;
    memmove(window->preedit + first + insertedLength, window->preedit + first + removed,
            tail * sizeof(uint32_t));
    memcpy(window->preedit + first, inserted, insertedLength * sizeof(uint32_t));
    window->preeditLength = first + insertedLength + tail;
    window->preeditCaret = draw->caret < 0 ? 0 : (size_t)draw->caret;
    push_preedit(window);
}

static void preedit_caret(XIC ic, XPointer client, XIMPreeditCaretCallbackStruct *caret) {
    (void)ic;
    X11Window *window = find((uint32_t)(uintptr_t)client);
    if (!window || !caret) return;
    if (caret->direction == XIMAbsolutePosition) window->preeditCaret = (size_t)caret->position;
    caret->position = (int)window->preeditCaret;
    push_preedit(window);
}

static void open_im(void) {
    const char *current = setlocale(LC_CTYPE, NULL);
    if (!current || strcmp(current, "C") == 0 || strcmp(current, "POSIX") == 0) {
        if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) setlocale(LC_CTYPE, "C.UTF-8");
    }
    if (!XSupportsLocale()) return;
    XSetLocaleModifiers("");
    im = XOpenIM(display, NULL, NULL, NULL);
    if (!im) {
        XSetLocaleModifiers("@im=none");
        im = XOpenIM(display, NULL, NULL, NULL);
    }
    if (!im) return;
    XIMStyles *styles = NULL;
    if (XGetIMValues(im, XNQueryInputStyle, &styles, NULL) || !styles) return;
    XIMStyle preferred[] = {
        XIMPreeditCallbacks | XIMStatusNothing,
        XIMPreeditPosition | XIMStatusNothing,
        XIMPreeditNothing | XIMStatusNothing,
    };
    for (size_t p = 0; p < 3 && !imStyle; p++)
        for (unsigned short i = 0; i < styles->count_styles; i++)
            if (styles->supported_styles[i] == preferred[p]) {
                imStyle = preferred[p];
                break;
            }
    XFree(styles);
}

static void create_ic(X11Window *window) {
    if (!im || !imStyle) return;
    XPointer client = (XPointer)(uintptr_t)window->id;
    if (imStyle & XIMPreeditCallbacks) {
        XIMCallback start = {client, (XIMProc)(void (*)(void))preedit_start};
        XIMCallback done = {client, (XIMProc)(void (*)(void))preedit_done};
        XIMCallback draw = {client, (XIMProc)(void (*)(void))preedit_draw};
        XIMCallback caret = {client, (XIMProc)(void (*)(void))preedit_caret};
        XVaNestedList attributes = XVaCreateNestedList(0, XNPreeditStartCallback, &start,
                                                       XNPreeditDoneCallback, &done,
                                                       XNPreeditDrawCallback, &draw,
                                                       XNPreeditCaretCallback, &caret, NULL);
        window->ic = XCreateIC(im, XNInputStyle, imStyle, XNClientWindow, window->xid,
                               XNFocusWindow, window->xid, XNPreeditAttributes, attributes,
                               NULL);
        XFree(attributes);
    } else if (imStyle & XIMPreeditPosition) {
        XPoint spot = {0, 0};
        XVaNestedList attributes = XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);
        window->ic = XCreateIC(im, XNInputStyle, imStyle, XNClientWindow, window->xid,
                               XNFocusWindow, window->xid, XNPreeditAttributes, attributes,
                               NULL);
        XFree(attributes);
    } else {
        window->ic = XCreateIC(im, XNInputStyle, imStyle, XNClientWindow, window->xid,
                               XNFocusWindow, window->xid, NULL);
    }
    if (window->ic) {
        long mask = 0;
        XGetICValues(window->ic, XNFilterEvents, &mask, NULL);
        XWindowAttributes attributes;
        XGetWindowAttributes(display, window->xid, &attributes);
        XSelectInput(display, window->xid, attributes.your_event_mask | mask);
        XUnsetICFocus(window->ic);
    }
}

static void update_ic_focus(X11Window *window) {
    if (!window->ic) return;
    bool want = window->focused && window->imeEnabled;
    if (want == window->icFocused) return;
    window->icFocused = want;
    if (want) {
        XSetICFocus(window->ic);
    } else {
        XUnsetICFocus(window->ic);
        char *rest = Xutf8ResetIC(window->ic);
        if (rest) XFree(rest);
        if (window->preeditLength) {
            window->preeditLength = window->preeditCaret = 0;
            push_preedit(window);
        }
    }
}

static void update_spot(X11Window *window) {
    if (!window->ic || !(imStyle & XIMPreeditPosition)) return;
    XPoint spot = {(short)lround(window->imeX * scale),
                   (short)lround((window->imeY + window->imeHeight) * scale)};
    XVaNestedList attributes = XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);
    XSetICValues(window->ic, XNPreeditAttributes, attributes, NULL);
    XFree(attributes);
}

// ---------------------------------------------------------------------------------------
// Start and windows.

bool silk_x11_start(void) {
    XInitThreads();
    display = XOpenDisplay(NULL);
    if (!display) return false;
    screen = DefaultScreen(display);
    root = RootWindow(display, screen);
    const char *names[] = {"WM_PROTOCOLS", "WM_DELETE_WINDOW", "_NET_WM_NAME", "UTF8_STRING",
                           "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_MOVERESIZE",
                           "_NET_ACTIVE_WINDOW", "_MOTIF_WM_HINTS", "CLIPBOARD", "TARGETS",
                           "SILK_SELECTION", "_NET_WM_PID", "RESOURCE_MANAGER", "TEXT"};
    Atom atoms[15];
    XInternAtoms(display, (char **)names, 15, False, atoms);
    WM_PROTOCOLS = atoms[0];
    WM_DELETE_WINDOW = atoms[1];
    NET_WM_NAME = atoms[2];
    UTF8_STRING = atoms[3];
    NET_WM_STATE = atoms[4];
    NET_WM_STATE_FULLSCREEN = atoms[5];
    NET_WM_MOVERESIZE = atoms[6];
    NET_ACTIVE_WINDOW = atoms[7];
    MOTIF_WM_HINTS = atoms[8];
    CLIPBOARD = atoms[9];
    TARGETS = atoms[10];
    SILK_SELECTION = atoms[11];
    NET_WM_PID = atoms[12];
    RESOURCE_MANAGER = atoms[13];
    TEXT_ATOM = atoms[14];
    Bool detectable = False;
    XkbSetDetectableAutoRepeat(display, True, &detectable);
    XrmInitialize();
    scale = read_scale();
    const char *hz = getenv("SILK_WINDOW_REFRESH_HZ");
    if (hz && atof(hz) > 0) refreshInterval = 1.0 / atof(hz);
    frameEpoch = silk_now();
    helper = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
    XSelectInput(display, root, PropertyChangeMask);
    open_im();
    return true;
}

uint32_t silk_x11_open(const uint8_t *title, size_t titleLength, double width, double height,
                       double minWidth, double minHeight, uint32_t flags) {
    if (windowCount == windowCapacity) {
        size_t capacity = windowCapacity ? windowCapacity * 2 : 8;
        X11Window *grown = realloc(windows, capacity * sizeof *grown);
        if (!grown) return 0;
        windows = grown;
        windowCapacity = capacity;
    }
    X11Window *window = &windows[windowCount];
    memset(window, 0, sizeof *window);
    window->pixelWidth = (int)lround(width * scale);
    window->pixelHeight = (int)lround(height * scale);
    if (window->pixelWidth < 1) window->pixelWidth = 1;
    if (window->pixelHeight < 1) window->pixelHeight = 1;

    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);
    XSetWindowAttributes attributes;
    memset(&attributes, 0, sizeof attributes);
    unsigned long mask = CWEventMask | CWColormap | CWBorderPixel | CWBackPixmap;
    if (flags & SILK_WINDOW_TRANSPARENT) {
        XVisualInfo info;
        if (XMatchVisualInfo(display, screen, 32, TrueColor, &info)) {
            visual = info.visual;
            depth = 32;
        }
    }
    attributes.colormap = XCreateColormap(display, root, visual, AllocNone);
    attributes.background_pixmap = None;
    attributes.border_pixel = 0;
    attributes.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                            PointerMotionMask | EnterWindowMask | LeaveWindowMask |
                            StructureNotifyMask | FocusChangeMask | ExposureMask |
                            PropertyChangeMask | VisibilityChangeMask;
    Window xid = XCreateWindow(display, root, 0, 0, (unsigned)window->pixelWidth,
                               (unsigned)window->pixelHeight, 0, depth, InputOutput, visual,
                               mask, &attributes);
    if (!xid) return 0;
    windowCount++;
    window->id = nextWindow++;
    window->xid = xid;
    window->colormap = attributes.colormap;
    window->open = true;
    window->minWidth = minWidth;
    window->minHeight = minHeight;
    window->resizable = (flags & SILK_WINDOW_RESIZABLE) != 0;
    XSetWMProtocols(display, xid, &WM_DELETE_WINDOW, 1);
    long pid = (long)getpid();
    XChangeProperty(display, xid, NET_WM_PID, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&pid, 1);
    XClassHint *classHint = XAllocClassHint();
    if (classHint) {
        classHint->res_name = "silk";
        classHint->res_class = "Silk";
        XSetClassHint(display, xid, classHint);
        XFree(classHint);
    }
    if (flags & SILK_WINDOW_TITLE_HIDDEN) {
        // _MOTIF_WM_HINTS: flags MWM_HINTS_DECORATIONS, decorations none.
        long hints[5] = {2, 0, 0, 0, 0};
        XChangeProperty(display, xid, MOTIF_WM_HINTS, MOTIF_WM_HINTS, 32, PropModeReplace,
                        (unsigned char *)hints, 5);
    }
    silk_x11_set_title(window->id, title, titleLength);
    apply_size_hints(window);
    create_ic(window);
    if (flags & SILK_WINDOW_VISIBLE) silk_x11_set_visible(window->id, true);
    XFlush(display);
    return window->id;
}

void silk_x11_close(uint32_t id) {
    X11Window *window = find(id);
    if (!window) return;
    if (window->ic) XDestroyIC(window->ic);
    XDestroyWindow(display, window->xid);
    XFreeColormap(display, window->colormap);
    window->open = false;
    XFlush(display);
}

// ---------------------------------------------------------------------------------------
// Clipboard.

static void answer_selection_request(XSelectionRequestEvent *request) {
    XSelectionEvent reply;
    memset(&reply, 0, sizeof reply);
    reply.type = SelectionNotify;
    reply.requestor = request->requestor;
    reply.selection = request->selection;
    reply.target = request->target;
    reply.time = request->time;
    reply.property = None;
    Atom property = request->property ? request->property : request->target;
    if (request->selection == CLIPBOARD && clipboardOwned) {
        if (request->target == TARGETS) {
            Atom targets[] = {TARGETS, UTF8_STRING, XA_STRING, TEXT_ATOM};
            XChangeProperty(display, request->requestor, property, XA_ATOM, 32,
                            PropModeReplace, (unsigned char *)targets, 4);
            reply.property = property;
        } else if (request->target == UTF8_STRING || request->target == XA_STRING ||
                   request->target == TEXT_ATOM) {
            XChangeProperty(display, request->requestor, property,
                            request->target == XA_STRING ? XA_STRING : UTF8_STRING, 8,
                            PropModeReplace, clipboardText, (int)clipboardLength);
            reply.property = property;
        }
    }
    XSendEvent(display, request->requestor, False, 0, (XEvent *)&reply);
}

static void handle_event(XEvent *event);

int64_t silk_x11_clipboard_read(uint8_t *destination, size_t capacity) {
    if (clipboardOwned) {
        if (destination && capacity)
            memcpy(destination, clipboardText, clipboardLength < capacity ? clipboardLength : capacity);
        return (int64_t)clipboardLength;
    }
    // A second call (with a buffer) reads what the first one fetched.
    if (destination && clipboardReadLength >= 0) {
        size_t length = (size_t)clipboardReadLength;
        if (capacity) memcpy(destination, clipboardRead, length < capacity ? length : capacity);
        return clipboardReadLength;
    }
    free(clipboardRead);
    clipboardRead = NULL;
    clipboardReadLength = -1;
    if (XGetSelectionOwner(display, CLIPBOARD) == None) return -1;
    XConvertSelection(display, CLIPBOARD, UTF8_STRING, SILK_SELECTION, helper, CurrentTime);
    XFlush(display);
    double deadline = silk_now() + 1.0;
    for (;;) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == SelectionNotify && event.xselection.requestor == helper) {
                if (event.xselection.property == None) return -1;
                Atom type;
                int format;
                unsigned long count, remaining;
                unsigned char *data = NULL;
                XGetWindowProperty(display, helper, SILK_SELECTION, 0, 0x7FFFFFFF, True,
                                   AnyPropertyType, &type, &format, &count, &remaining, &data);
                if (!data || format != 8) {
                    if (data) XFree(data);
                    return -1;
                }
                clipboardRead = malloc(count ? count : 1);
                if (!clipboardRead) abort();
                memcpy(clipboardRead, data, count);
                clipboardReadLength = (int64_t)count;
                XFree(data);
                if (destination && capacity) memcpy(destination, clipboardRead, count < capacity ? count : capacity);
                return clipboardReadLength;
            }
            if (!XFilterEvent(&event, None)) handle_event(&event);
        }
        double now = silk_now();
        if (now >= deadline) return -1;
        struct pollfd fd = {ConnectionNumber(display), POLLIN, 0};
        poll(&fd, 1, (int)ceil((deadline - now) * 1000.0));
    }
}

void silk_x11_clipboard_write(const uint8_t *text, size_t length) {
    free(clipboardText);
    clipboardText = malloc(length ? length : 1);
    if (!clipboardText) abort();
    if (length) memcpy(clipboardText, text, length);
    clipboardLength = length;
    XSetSelectionOwner(display, CLIPBOARD, helper, CurrentTime);
    clipboardOwned = XGetSelectionOwner(display, CLIPBOARD) == helper;
    XFlush(display);
}

// ---------------------------------------------------------------------------------------
// Events.

static void push_modifiers(X11Window *window, uint32_t modifiers) {
    SilkWindowEvent event = silk_event(SILK_EVENT_MODIFIERS_CHANGED, window->id);
    event.modifiers = modifiers;
    silk_queue_push(&event);
}

static size_t keysym_utf8(KeySym keysym, uint8_t out[8]) {
    char buffer[8] = {0};
    int length = xkb_keysym_to_utf8((xkb_keysym_t)keysym, buffer, sizeof buffer);
    if (length <= 1) return 0; // includes the terminator
    memcpy(out, buffer, (size_t)length - 1);
    return (size_t)length - 1;
}

static void handle_key(X11Window *window, XKeyEvent *key, bool filtered) {
    bool down = key->type == KeyPress;
    // An input method's commit arrives as a key press with key code 0.
    if (down && key->keycode == 0) {
        if (window->ic) {
            char buffer[512];
            KeySym keysym;
            Status status;
            int length = Xutf8LookupString(window->ic, key, buffer, sizeof buffer - 1, &keysym, &status);
            if (status == XLookupChars || status == XLookupBoth)
                push_commit(window, (uint8_t *)buffer, (size_t)length);
        }
        return;
    }
    uint32_t evdev = key->keycode >= 8 ? key->keycode - 8 : 0;
    uint32_t code = silk_evdev_code(evdev);
    uint32_t modifiers = modifiers_from_state(key->state);
    int group = (int)((key->state >> 13) & 3);
    KeySym base = XkbKeycodeToKeysym(display, (KeyCode)key->keycode, group, 0);
    KeySym shifted = XkbKeycodeToKeysym(display, (KeyCode)key->keycode, group,
                                        (key->state & ShiftMask) ? 1 : 0);
    if (shifted == NoSymbol) shifted = base;
    KeySym actual = shifted;
    {
        XKeyEvent copy = *key;
        char ignored[8];
        XLookupString(&copy, ignored, sizeof ignored, &actual, NULL);
        if (actual == NoSymbol) actual = shifted;
    }
    uint32_t named = silk_keysym_named((uint32_t)actual);
    if (!named) named = silk_keysym_named((uint32_t)base);
    if (!named && code < SILK_CODE_COUNT) named = silk_code_named_keys[code];
    uint8_t keyText[8];
    size_t keyTextLength = named ? 0 : keysym_utf8(shifted, keyText);
    uint32_t modifierBit = silk_named_modifier(named);

    if (!down) {
        if (evdev < 256) pressed[evdev / 8] &= (uint8_t)~(1u << (evdev % 8));
        SilkWindowEvent event = silk_event(SILK_EVENT_KEY_UP, window->id);
        event.code = code;
        event.named = named;
        event.modifiers = modifiers;
        silk_queue_push_text(&event, keyText, keyTextLength);
        if (modifierBit && modifierBit != SILK_MOD_CAPS_LOCK) push_modifiers(window, modifiers & ~modifierBit);
        return;
    }

    bool repeat = evdev < 256 && (pressed[evdev / 8] & (1u << (evdev % 8)));
    if (evdev < 256) pressed[evdev / 8] |= (uint8_t)(1u << (evdev % 8));

    if (!filtered && (modifiers & (SILK_MOD_CONTROL | SILK_MOD_META | SILK_MOD_ALT))) {
        uint8_t baseText[8];
        size_t baseLength = keysym_utf8(base, baseText);
        uint32_t character = baseLength ? silk_utf8_first(baseText, baseLength) : 0;
        if (silk_linux_shortcut(window->id, character, named, modifiers)) return;
    }

    SilkWindowEvent event = silk_event(SILK_EVENT_KEY_DOWN, window->id);
    event.code = code;
    event.named = named;
    event.modifiers = modifiers;
    if (repeat) event.flags |= SILK_FLAG_REPEAT;
    if (filtered) event.flags |= SILK_FLAG_IME_HANDLED;
    silk_queue_push_text(&event, keyText, keyTextLength);
    if (modifierBit) {
        uint32_t after = modifierBit == SILK_MOD_CAPS_LOCK ? modifiers ^ modifierBit
                                                           : modifiers | modifierBit;
        push_modifiers(window, after);
    }
    if (filtered) return;
    if (modifiers & (SILK_MOD_CONTROL | SILK_MOD_META)) return;

    // The text the key types, through the input method context when it is on.
    uint8_t typed[512];
    size_t typedLength = 0;
    if (window->icFocused && window->ic) {
        KeySym keysym;
        Status status;
        int length = Xutf8LookupString(window->ic, key, (char *)typed, sizeof typed - 1, &keysym, &status);
        if ((status == XLookupChars || status == XLookupBoth) && length > 0) typedLength = (size_t)length;
    } else {
        typedLength = keysym_utf8(actual, typed);
    }
    push_commit(window, typed, typedLength);
}

static uint32_t button_of(unsigned button) {
    switch (button) {
    case Button1: return SILK_BUTTON_LEFT;
    case Button2: return SILK_BUTTON_MIDDLE;
    case Button3: return SILK_BUTTON_RIGHT;
    case 8: return SILK_BUTTON_BACK;
    case 9: return SILK_BUTTON_FORWARD;
    default: return SILK_BUTTON_OTHER;
    }
}

static void handle_button(X11Window *window, XButtonEvent *button) {
    double x = button->x / scale, y = button->y / scale;
    uint32_t modifiers = modifiers_from_state(button->state);
    if (button->button >= 4 && button->button <= 7) {
        if (button->type != ButtonPress) return;
        SilkWindowEvent event = silk_event(SILK_EVENT_SCROLL, window->id);
        event.x = x;
        event.y = y;
        event.modifiers = modifiers;
        if (button->button == 4) event.dy = 1;
        if (button->button == 5) event.dy = -1;
        if (button->button == 6) event.dx = 1;
        if (button->button == 7) event.dx = -1;
        silk_queue_push(&event);
        return;
    }
    if (button->type == ButtonPress) {
        double now = silk_now();
        bool same = window->lastClickButton == button->button && now - window->lastClickTime < 0.5 &&
                    abs(button->x - window->lastClickX) <= 4 && abs(button->y - window->lastClickY) <= 4;
        window->clickCount = same ? window->clickCount + 1 : 1;
        window->lastClickTime = now;
        window->lastClickX = button->x;
        window->lastClickY = button->y;
        window->lastClickButton = button->button;
    }
    SilkWindowEvent event = silk_event(button->type == ButtonPress ? SILK_EVENT_MOUSE_DOWN
                                                                   : SILK_EVENT_MOUSE_UP,
                                       window->id);
    event.code = button_of(button->button);
    event.x = x;
    event.y = y;
    event.modifiers = modifiers;
    event.count = window->clickCount;
    silk_queue_push(&event);
}

static bool read_full_screen(X11Window *window) {
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char *data = NULL;
    bool full = false;
    if (XGetWindowProperty(display, window->xid, NET_WM_STATE, 0, 64, False, XA_ATOM, &type,
                           &format, &count, &remaining, &data) == Success && data) {
        Atom *atoms = (Atom *)data;
        for (unsigned long i = 0; i < count; i++)
            if (atoms[i] == NET_WM_STATE_FULLSCREEN) full = true;
        XFree(data);
    }
    return full;
}

static void rescale(void) {
    double updated = read_scale();
    if (fabs(updated - scale) < 1e-9) return;
    scale = updated;
    for (size_t i = 0; i < windowCount; i++) {
        X11Window *window = &windows[i];
        if (!window->open) continue;
        SilkWindowEvent event = silk_event(SILK_EVENT_SCALE_FACTOR_CHANGED, window->id);
        event.value = scale;
        silk_queue_push(&event);
        push_resized(window);
        apply_size_hints(window);
    }
}

static void handle_event(XEvent *event) {
    if (event->type == SelectionRequest) {
        answer_selection_request(&event->xselectionrequest);
        return;
    }
    if (event->type == SelectionClear) {
        if (event->xselectionclear.selection == CLIPBOARD) clipboardOwned = false;
        return;
    }
    if (event->type == PropertyNotify && event->xproperty.window == root) {
        if (event->xproperty.atom == RESOURCE_MANAGER) rescale();
        return;
    }
    X11Window *window = find_xid(event->xany.window);
    if (!window) return;
    switch (event->type) {
    case KeyPress:
    case KeyRelease:
        handle_key(window, &event->xkey, false);
        break;
    case ButtonPress:
    case ButtonRelease:
        handle_button(window, &event->xbutton);
        break;
    case MotionNotify: {
        SilkWindowEvent out = silk_event(SILK_EVENT_MOUSE_MOVED, window->id);
        out.x = event->xmotion.x / scale;
        out.y = event->xmotion.y / scale;
        out.modifiers = modifiers_from_state(event->xmotion.state);
        silk_queue_push(&out);
        break;
    }
    case EnterNotify: {
        SilkWindowEvent out = silk_event(SILK_EVENT_MOUSE_ENTERED, window->id);
        out.x = event->xcrossing.x / scale;
        out.y = event->xcrossing.y / scale;
        silk_queue_push(&out);
        break;
    }
    case LeaveNotify: {
        SilkWindowEvent out = silk_event(SILK_EVENT_MOUSE_EXITED, window->id);
        silk_queue_push(&out);
        break;
    }
    case FocusIn:
    case FocusOut: {
        if (event->xfocus.mode == NotifyGrab || event->xfocus.mode == NotifyUngrab) break;
        bool focused = event->type == FocusIn;
        if (focused == window->focused) break;
        window->focused = focused;
        if (!focused) memset(pressed, 0, sizeof pressed);
        update_ic_focus(window);
        SilkWindowEvent out = silk_event(SILK_EVENT_FOCUSED, window->id);
        if (focused) out.flags |= SILK_FLAG_ON;
        silk_queue_push(&out);
        break;
    }
    case ConfigureNotify: {
        XConfigureEvent *configure = &event->xconfigure;
        if (configure->width != window->pixelWidth || configure->height != window->pixelHeight) {
            window->pixelWidth = configure->width;
            window->pixelHeight = configure->height;
            push_resized(window);
        }
        int x = configure->x, y = configure->y;
        if (!configure->send_event) {
            Window child;
            XTranslateCoordinates(display, window->xid, root, 0, 0, &x, &y, &child);
        }
        if (x != window->x || y != window->y) {
            window->x = x;
            window->y = y;
            SilkWindowEvent out = silk_event(SILK_EVENT_MOVED, window->id);
            out.x = x / scale;
            out.y = y / scale;
            silk_queue_push(&out);
        }
        break;
    }
    case MapNotify:
        window->mapped = true;
        force_frame(window);
        break;
    case UnmapNotify:
        window->mapped = false;
        break;
    case Expose:
        if (event->xexpose.count == 0) force_frame(window);
        break;
    case VisibilityNotify: {
        SilkWindowEvent out = silk_event(SILK_EVENT_OCCLUDED, window->id);
        if (event->xvisibility.state == VisibilityFullyObscured) out.flags |= SILK_FLAG_ON;
        silk_queue_push(&out);
        if (event->xvisibility.state != VisibilityFullyObscured) force_frame(window);
        break;
    }
    case PropertyNotify:
        if (event->xproperty.atom == NET_WM_STATE) {
            bool full = read_full_screen(window);
            if (full != window->fullScreen) {
                window->fullScreen = full;
                SilkWindowEvent out = silk_event(SILK_EVENT_FULL_SCREEN_CHANGED, window->id);
                if (full) out.flags |= SILK_FLAG_ON;
                silk_queue_push(&out);
            }
        }
        break;
    case ClientMessage:
        if (event->xclient.message_type == WM_PROTOCOLS &&
            (Atom)event->xclient.data.l[0] == WM_DELETE_WINDOW) {
            SilkWindowEvent out = silk_event(SILK_EVENT_CLOSE_REQUESTED, window->id);
            silk_queue_push(&out);
        }
        break;
    default:
        break;
    }
}

void silk_x11_dispatch(void) {
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
        bool filtered = XFilterEvent(&event, None);
        if (filtered) {
            // The input method used the key; still report the press.
            if (event.type == KeyPress) {
                X11Window *window = find_xid(event.xkey.window);
                if (window && event.xkey.keycode) handle_key(window, &event.xkey, true);
            }
            continue;
        }
        handle_event(&event);
    }
}

int silk_x11_fd(void) { return ConnectionNumber(display); }

bool silk_x11_prepare_wait(void) {
    XFlush(display);
    return XPending(display) == 0;
}

void silk_x11_cancel_wait(void) {}

void silk_x11_finish_wait(bool readable) { (void)readable; }

static double next_tick(double now) {
    return frameEpoch + (floor((now - frameEpoch) / refreshInterval) + 1.0) * refreshInterval;
}

double silk_x11_frames(double now) {
    double next = INFINITY;
    for (size_t i = 0; i < windowCount; i++) {
        X11Window *window = &windows[i];
        if (!window->open || !window->frame.pending || !window->visible) continue;
        if (!window->frame.forced && now < window->due) {
            if (window->due < next) next = window->due;
            continue;
        }
        window->frame.pending = window->frame.forced = false;
        SilkWindowEvent event = silk_event(SILK_EVENT_REQUEST_FRAME, window->id);
        event.value = next_tick(now);
        silk_queue_push(&event);
    }
    return next;
}

void silk_x11_request_frame(uint32_t id) {
    X11Window *window = find(id);
    if (!window || window->frame.pending) return;
    window->frame.pending = true;
    window->due = next_tick(silk_now());
}

void silk_x11_content_size(uint32_t id, double *width, double *height) {
    X11Window *window = find(id);
    if (!window) return;
    *width = window->pixelWidth / scale;
    *height = window->pixelHeight / scale;
}

double silk_x11_scale_factor(uint32_t id) {
    (void)id;
    return scale;
}

uint32_t silk_x11_raw_surface(uint32_t id, void **first, void **second, uint64_t *xid) {
    (void)second;
    X11Window *window = find(id);
    if (!window) return SILK_SURFACE_NONE;
    *first = display;
    *xid = (uint64_t)window->xid;
    return SILK_SURFACE_XLIB;
}

void silk_x11_set_title(uint32_t id, const uint8_t *title, size_t length) {
    X11Window *window = find(id);
    if (!window) return;
    XChangeProperty(display, window->xid, NET_WM_NAME, UTF8_STRING, 8, PropModeReplace, title,
                    (int)length);
    char *copy = silk_strndup(title, length);
    XStoreName(display, window->xid, copy);
    free(copy);
    XFlush(display);
}

void silk_x11_set_content_size(uint32_t id, double width, double height) {
    X11Window *window = find(id);
    if (!window) return;
    int w = (int)lround(width * scale), h = (int)lround(height * scale);
    if (!window->resizable) {
        window->pixelWidth = w;
        window->pixelHeight = h;
        apply_size_hints(window);
    }
    XResizeWindow(display, window->xid, (unsigned)(w < 1 ? 1 : w), (unsigned)(h < 1 ? 1 : h));
    XFlush(display);
}

void silk_x11_set_min_content_size(uint32_t id, double width, double height) {
    X11Window *window = find(id);
    if (!window) return;
    window->minWidth = width;
    window->minHeight = height;
    apply_size_hints(window);
    XFlush(display);
}

void silk_x11_set_visible(uint32_t id, bool visible) {
    X11Window *window = find(id);
    if (!window || window->visible == visible) return;
    window->visible = visible;
    if (visible) {
        XMapRaised(display, window->xid);
        push_resized(window);
    } else {
        XUnmapWindow(display, window->xid);
        window->frame.pending = false;
    }
    XFlush(display);
}

void silk_x11_focus(uint32_t id) {
    X11Window *window = find(id);
    if (!window) return;
    XRaiseWindow(display, window->xid);
    XEvent event;
    memset(&event, 0, sizeof event);
    event.xclient.type = ClientMessage;
    event.xclient.window = window->xid;
    event.xclient.message_type = NET_ACTIVE_WINDOW;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // from an application
    event.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &event);
    if (window->mapped) XSetInputFocus(display, window->xid, RevertToParent, CurrentTime);
    XFlush(display);
}

void silk_x11_minimize(uint32_t id) {
    X11Window *window = find(id);
    if (!window) return;
    XIconifyWindow(display, window->xid, screen);
    XFlush(display);
}

void silk_x11_toggle_full_screen(uint32_t id) {
    X11Window *window = find(id);
    if (!window) return;
    XEvent event;
    memset(&event, 0, sizeof event);
    event.xclient.type = ClientMessage;
    event.xclient.window = window->xid;
    event.xclient.message_type = NET_WM_STATE;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2; // toggle
    event.xclient.data.l[1] = (long)NET_WM_STATE_FULLSCREEN;
    event.xclient.data.l[3] = 1;
    XSendEvent(display, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &event);
    XFlush(display);
}

void silk_x11_start_window_drag(uint32_t id) {
    X11Window *window = find(id);
    if (!window) return;
    Window rootReturn, child;
    int rootX = 0, rootY = 0, x, y;
    unsigned mask;
    XQueryPointer(display, window->xid, &rootReturn, &child, &rootX, &rootY, &x, &y, &mask);
    XUngrabPointer(display, CurrentTime);
    XEvent event;
    memset(&event, 0, sizeof event);
    event.xclient.type = ClientMessage;
    event.xclient.window = window->xid;
    event.xclient.message_type = NET_WM_MOVERESIZE;
    event.xclient.format = 32;
    event.xclient.data.l[0] = rootX;
    event.xclient.data.l[1] = rootY;
    event.xclient.data.l[2] = 8; // _NET_WM_MOVERESIZE_MOVE
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 1;
    XSendEvent(display, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &event);
    XFlush(display);
}

static Cursor cursor_for(uint32_t cursor) {
    if (cursor >= 32) return None;
    if (cursor == 21) { // Cursor.None
        if (!blankCursor) {
            char data = 0;
            XColor black = {0};
            Pixmap pixmap = XCreateBitmapFromData(display, root, &data, 1, 1);
            blankCursor = XCreatePixmapCursor(display, pixmap, pixmap, &black, &black, 0, 0);
            XFreePixmap(display, pixmap);
        }
        return blankCursor;
    }
    static const unsigned shapes[] = {
        XC_left_ptr,          // Arrow
        XC_xterm,             // IBeam
        XC_hand2,             // PointingHand
        XC_crosshair,         // Crosshair
        XC_fleur,             // ClosedHand
        XC_hand1,             // OpenHand
        XC_X_cursor,          // NotAllowed
        XC_sb_h_double_arrow, // ResizeLeftRight
        XC_sb_v_double_arrow, // ResizeUpDown
        XC_left_side,         // ResizeLeft
        XC_right_side,        // ResizeRight
        XC_top_side,          // ResizeUp
        XC_bottom_side,       // ResizeDown
        XC_bottom_right_corner, // ResizeNorthWestSouthEast
        XC_bottom_left_corner,  // ResizeNorthEastSouthWest
        XC_sb_h_double_arrow, // ResizeColumn
        XC_sb_v_double_arrow, // ResizeRow
        XC_xterm,             // IBeamVertical
        XC_plus,              // DragCopy
        XC_left_ptr,          // DragLink
        XC_left_ptr,          // ContextualMenu
    };
    if (cursor >= sizeof shapes / sizeof shapes[0]) return None;
    if (!cursors[cursor]) cursors[cursor] = XCreateFontCursor(display, shapes[cursor]);
    return cursors[cursor];
}

void silk_x11_set_cursor(uint32_t id, uint32_t cursor) {
    X11Window *window = find(id);
    if (!window) return;
    window->cursor = cursor;
    XDefineCursor(display, window->xid, cursor_for(cursor));
    XFlush(display);
}

void silk_x11_set_ime(uint32_t id, bool enabled, double x, double y, double width, double height) {
    X11Window *window = find(id);
    if (!window) return;
    window->imeEnabled = enabled;
    window->imeX = x;
    window->imeY = y;
    window->imeWidth = width;
    window->imeHeight = height;
    update_ic_focus(window);
    update_spot(window);
    XFlush(display);
}

// ---------------------------------------------------------------------------------------
// Test hooks: synthetic events sent through the X server.

uint8_t silk_x11_test_key(uint32_t id, uint32_t code, const uint8_t *text, size_t length,
                          uint32_t modifiers) {
    (void)text;
    (void)length;
    X11Window *window = find(id);
    uint32_t evdev = silk_evdev_from_code(code);
    if (!window || !evdev) return 0;
    for (int down = 1; down >= 0; down--) {
        XEvent event;
        memset(&event, 0, sizeof event);
        event.xkey.type = down ? KeyPress : KeyRelease;
        event.xkey.display = display;
        event.xkey.window = window->xid;
        event.xkey.root = root;
        event.xkey.subwindow = None;
        event.xkey.time = CurrentTime;
        event.xkey.same_screen = True;
        event.xkey.keycode = evdev + 8;
        event.xkey.state = state_from_modifiers(modifiers);
        XSendEvent(display, window->xid, True, down ? KeyPressMask : KeyReleaseMask, &event);
    }
    XFlush(display);
    return 1;
}

uint8_t silk_x11_test_mouse(uint32_t id, uint32_t kind, uint32_t button, double x, double y,
                            double dx, double dy) {
    X11Window *window = find(id);
    if (!window) return 0;
    static const unsigned buttons[] = {Button1, Button3, Button2, 8, 9, 10};
    XEvent event;
    memset(&event, 0, sizeof event);
    int px = (int)lround(x * scale), py = (int)lround(y * scale);
    if (kind == 0) {
        event.xmotion.type = MotionNotify;
        event.xmotion.display = display;
        event.xmotion.window = window->xid;
        event.xmotion.root = root;
        event.xmotion.x = px;
        event.xmotion.y = py;
        event.xmotion.same_screen = True;
        XSendEvent(display, window->xid, True, PointerMotionMask, &event);
    } else {
        unsigned which = button < 6 ? buttons[button] : Button1;
        int presses = 1;
        if (kind == 3) {
            which = dy > 0 ? Button4 : dy < 0 ? Button5 : dx > 0 ? 6 : 7;
            presses = (int)lround(fabs(dy != 0 ? dy : dx));
        }
        for (int i = 0; i < presses; i++) {
            for (int phase = 0; phase < 2; phase++) {
                bool press = kind == 1 || (kind == 3 && phase == 0);
                if (kind != 3 && phase == 1) break;
                event.xbutton.type = press ? ButtonPress : ButtonRelease;
                event.xbutton.display = display;
                event.xbutton.window = window->xid;
                event.xbutton.root = root;
                event.xbutton.x = px;
                event.xbutton.y = py;
                event.xbutton.button = which;
                event.xbutton.same_screen = True;
                XSendEvent(display, window->xid, True,
                           press ? ButtonPressMask : ButtonReleaseMask, &event);
            }
        }
    }
    XFlush(display);
    return 1;
}

uint8_t silk_x11_test_preedit(uint32_t id, const uint8_t *text, size_t length, uint64_t start,
                              uint64_t end) {
    (void)end;
    X11Window *window = find(id);
    if (!window) return 0;
    // As the preedit draw callback would: replace the whole composition.
    window->preeditLength = decode_utf8((const char *)text, length, window->preedit, 256);
    size_t caret = 0;
    for (size_t i = 0, bytes = 0; i < window->preeditLength; i++) {
        if (bytes >= start) break;
        uint8_t scratch[4];
        bytes += silk_utf8_encode(window->preedit[i], scratch);
        caret = i + 1;
    }
    window->preeditCaret = caret;
    push_preedit(window);
    return 1;
}

uint8_t silk_x11_test_commit(uint32_t id, const uint8_t *text, size_t length) {
    X11Window *window = find(id);
    if (!window) return 0;
    if (window->preeditLength) {
        window->preeditLength = window->preeditCaret = 0;
        push_preedit(window);
    }
    push_commit(window, text, length);
    return 1;
}

uint8_t silk_x11_test_resize(uint32_t id, double width, double height) {
    X11Window *window = find(id);
    if (!window) return 0;
    XResizeWindow(display, window->xid, (unsigned)lround(width * scale),
                  (unsigned)lround(height * scale));
    XSync(display, False);
    return 1;
}

void silk_x11_test_ime_rect(uint32_t id, double *x, double *y, double *width, double *height) {
    X11Window *window = find(id);
    if (!window) return;
    *x = window->imeX;
    *y = window->imeY;
    *width = window->imeWidth;
    *height = window->imeHeight;
}
