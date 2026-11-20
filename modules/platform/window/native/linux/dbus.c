// The session bus half of the Linux backend, the same under X11 and Wayland:
//
// - Notifications: org.freedesktop.Notifications. The server's ids are mapped to ours,
//   and its ActionInvoked signal for the "default" action is a click.
// - The dock badge and progress: com.canonical.Unity.LauncherEntry, which Ubuntu's dock, KDE's
//   task manager, Plank and Dash to Dock read. The entry names the app's desktop file.
// - File dialogs: org.freedesktop.portal.FileChooser, which works inside Flatpak too.
// - The tray icon: a StatusNotifierItem registered with the StatusNotifierWatcher, which
//   KDE, most panels and GNOME's AppIndicator extension host. Its menu is exported with
//   com.canonical.dbusmenu.
//
// libdbus is loaded with dlopen when first needed, so programs that never use these do not
// depend on it, and a system without it simply has no tray or notifications.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "linux.h"

// The parts of the libdbus ABI used here (dbus/dbus.h). The iterator is larger than libdbus's
// own, which only writes into the front of it.
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct DBusMessageIter {
    void *opaque[16];
} DBusMessageIter;
typedef struct DBusError {
    const char *name;
    const char *message;
    void *opaque[4];
} DBusError;
typedef uint32_t dbus_bool_t;

enum { BUS_SESSION = 0 };
enum { MESSAGE_METHOD_CALL = 1, MESSAGE_METHOD_RETURN = 2, MESSAGE_ERROR = 3, MESSAGE_SIGNAL = 4 };
enum { NAME_DO_NOT_QUEUE = 4 };

#define T_STRING 's'
#define T_INT32 'i'
#define T_UINT32 'u'
#define T_BOOLEAN 'b'
#define T_INT64 'x'
#define T_DOUBLE 'd'
#define T_BYTE 'y'
#define T_OBJECT_PATH 'o'
#define T_ARRAY 'a'
#define T_VARIANT 'v'
#define T_STRUCT 'r'
#define T_DICT_ENTRY 'e'

static struct {
    void (*error_init)(DBusError *);
    void (*error_free)(DBusError *);
    DBusConnection *(*bus_get_private)(int, DBusError *);
    int (*bus_request_name)(DBusConnection *, const char *, unsigned, DBusError *);
    void (*bus_add_match)(DBusConnection *, const char *, DBusError *);
    void (*connection_set_exit_on_disconnect)(DBusConnection *, dbus_bool_t);
    dbus_bool_t (*connection_get_unix_fd)(DBusConnection *, int *);
    dbus_bool_t (*connection_read_write)(DBusConnection *, int);
    DBusMessage *(*connection_pop_message)(DBusConnection *);
    dbus_bool_t (*connection_send)(DBusConnection *, DBusMessage *, uint32_t *);
    void (*connection_flush)(DBusConnection *);
    DBusMessage *(*connection_send_with_reply_and_block)(DBusConnection *, DBusMessage *, int,
                                                          DBusError *);
    const char *(*bus_get_unique_name)(DBusConnection *);
    DBusMessage *(*message_new_method_call)(const char *, const char *, const char *,
                                            const char *);
    DBusMessage *(*message_new_method_return)(DBusMessage *);
    DBusMessage *(*message_new_error)(DBusMessage *, const char *, const char *);
    DBusMessage *(*message_new_signal)(const char *, const char *, const char *);
    void (*message_unref)(DBusMessage *);
    int (*message_get_type)(DBusMessage *);
    const char *(*message_get_interface)(DBusMessage *);
    const char *(*message_get_member)(DBusMessage *);
    const char *(*message_get_path)(DBusMessage *);
    uint32_t (*message_get_reply_serial)(DBusMessage *);
    void (*message_set_no_reply)(DBusMessage *, dbus_bool_t);
    dbus_bool_t (*message_iter_init)(DBusMessage *, DBusMessageIter *);
    void (*message_iter_init_append)(DBusMessage *, DBusMessageIter *);
    dbus_bool_t (*message_iter_append_basic)(DBusMessageIter *, int, const void *);
    dbus_bool_t (*message_iter_append_fixed_array)(DBusMessageIter *, int, const void *, int);
    dbus_bool_t (*message_iter_open_container)(DBusMessageIter *, int, const char *,
                                               DBusMessageIter *);
    dbus_bool_t (*message_iter_close_container)(DBusMessageIter *, DBusMessageIter *);
    int (*message_iter_get_arg_type)(DBusMessageIter *);
    void (*message_iter_get_basic)(DBusMessageIter *, void *);
    dbus_bool_t (*message_iter_next)(DBusMessageIter *);
    void (*message_iter_recurse)(DBusMessageIter *, DBusMessageIter *);
} dbus;

static DBusConnection *bus;
static int busFd = -1;
static bool loadTried;

static bool load(void) {
    if (loadTried) return bus != NULL;
    loadTried = true;
    void *library = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
    if (!library) return false;
#define BIND(field, name)                                                                     \
    if (!(*(void **)&dbus.field = dlsym(library, name))) return false;
    BIND(error_init, "dbus_error_init")
    BIND(error_free, "dbus_error_free")
    BIND(bus_get_private, "dbus_bus_get_private")
    BIND(bus_request_name, "dbus_bus_request_name")
    BIND(bus_add_match, "dbus_bus_add_match")
    BIND(connection_set_exit_on_disconnect, "dbus_connection_set_exit_on_disconnect")
    BIND(connection_get_unix_fd, "dbus_connection_get_unix_fd")
    BIND(connection_read_write, "dbus_connection_read_write")
    BIND(connection_pop_message, "dbus_connection_pop_message")
    BIND(connection_send, "dbus_connection_send")
    BIND(connection_flush, "dbus_connection_flush")
    BIND(connection_send_with_reply_and_block, "dbus_connection_send_with_reply_and_block")
    BIND(bus_get_unique_name, "dbus_bus_get_unique_name")
    BIND(message_new_method_call, "dbus_message_new_method_call")
    BIND(message_new_method_return, "dbus_message_new_method_return")
    BIND(message_new_error, "dbus_message_new_error")
    BIND(message_new_signal, "dbus_message_new_signal")
    BIND(message_unref, "dbus_message_unref")
    BIND(message_get_type, "dbus_message_get_type")
    BIND(message_get_interface, "dbus_message_get_interface")
    BIND(message_get_member, "dbus_message_get_member")
    BIND(message_get_path, "dbus_message_get_path")
    BIND(message_get_reply_serial, "dbus_message_get_reply_serial")
    BIND(message_set_no_reply, "dbus_message_set_no_reply")
    BIND(message_iter_init, "dbus_message_iter_init")
    BIND(message_iter_init_append, "dbus_message_iter_init_append")
    BIND(message_iter_append_basic, "dbus_message_iter_append_basic")
    BIND(message_iter_append_fixed_array, "dbus_message_iter_append_fixed_array")
    BIND(message_iter_open_container, "dbus_message_iter_open_container")
    BIND(message_iter_close_container, "dbus_message_iter_close_container")
    BIND(message_iter_get_arg_type, "dbus_message_iter_get_arg_type")
    BIND(message_iter_get_basic, "dbus_message_iter_get_basic")
    BIND(message_iter_next, "dbus_message_iter_next")
    BIND(message_iter_recurse, "dbus_message_iter_recurse")
#undef BIND
    DBusError error;
    dbus.error_init(&error);
    bus = dbus.bus_get_private(BUS_SESSION, &error);
    dbus.error_free(&error);
    if (!bus) return false;
    dbus.connection_set_exit_on_disconnect(bus, 0);
    if (!dbus.connection_get_unix_fd(bus, &busFd)) busFd = -1;
    dbus.error_init(&error);
    dbus.bus_add_match(bus,
                       "type='signal',interface='org.freedesktop.Notifications',"
                       "path='/org/freedesktop/Notifications'",
                       &error);
    dbus.error_free(&error);
    return true;
}

int silk_dbus_fd(void) { return bus ? busFd : -1; }

// Appending.

static void append_string(DBusMessageIter *iter, const char *text) {
    dbus.message_iter_append_basic(iter, T_STRING, &text);
}

static void append_bytes_string(DBusMessageIter *iter, const uint8_t *bytes, size_t length) {
    char *text = silk_strndup(bytes, length);
    append_string(iter, text ? text : "");
    free(text);
}

// A variant holding one basic value.
static void append_variant(DBusMessageIter *iter, int type, const void *value) {
    char signature[2] = {(char)type, 0};
    DBusMessageIter variant;
    dbus.message_iter_open_container(iter, T_VARIANT, signature, &variant);
    dbus.message_iter_append_basic(&variant, type, value);
    dbus.message_iter_close_container(iter, &variant);
}

static void append_entry(DBusMessageIter *dict, const char *key, int type, const void *value) {
    DBusMessageIter entry;
    dbus.message_iter_open_container(dict, T_DICT_ENTRY, NULL, &entry);
    append_string(&entry, key);
    append_variant(&entry, type, value);
    dbus.message_iter_close_container(dict, &entry);
}

static void send_and_unref(DBusMessage *message) {
    if (!message) return;
    dbus.connection_send(bus, message, NULL);
    dbus.message_unref(message);
}

// ---------------------------------------------------------------------------------------
// Notifications.

typedef struct NotificationMap {
    uint32_t ours;
    uint32_t serial; // the Notify call, until its reply names the server's id
    uint32_t server;
} NotificationMap;

static NotificationMap notifications[64];
static size_t notificationNext;
static uint32_t nextNotification = 1;

uint32_t silk_window_notify(const uint8_t *title, size_t titleLength, const uint8_t *body,
                            size_t bodyLength) {
    if (!load()) return 0;
    DBusMessage *call = dbus.message_new_method_call(
        "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications", "Notify");
    if (!call) return 0;
    DBusMessageIter args, actions, hints;
    dbus.message_iter_init_append(call, &args);
    const char *application = program_invocation_short_name;
    uint32_t replaces = 0;
    int32_t timeout = -1;
    append_string(&args, application);
    dbus.message_iter_append_basic(&args, T_UINT32, &replaces);
    append_string(&args, "");
    append_bytes_string(&args, title, titleLength);
    append_bytes_string(&args, body, bodyLength);
    dbus.message_iter_open_container(&args, T_ARRAY, "s", &actions);
    // The default action is what a click on the notification itself invokes.
    append_string(&actions, "default");
    append_string(&actions, "Open");
    dbus.message_iter_close_container(&args, &actions);
    dbus.message_iter_open_container(&args, T_ARRAY, "{sv}", &hints);
    dbus.message_iter_close_container(&args, &hints);
    dbus.message_iter_append_basic(&args, T_INT32, &timeout);
    uint32_t serial = 0;
    bool sent = dbus.connection_send(bus, call, &serial);
    dbus.message_unref(call);
    if (!sent) return 0;
    dbus.connection_flush(bus);
    uint32_t id = nextNotification++;
    NotificationMap *slot = &notifications[notificationNext++ % 64];
    slot->ours = id;
    slot->serial = serial;
    slot->server = 0;
    return id;
}

static void notify_replied(DBusMessage *reply) {
    uint32_t serial = dbus.message_get_reply_serial(reply);
    for (size_t i = 0; i < 64; i++) {
        if (notifications[i].serial != serial || !notifications[i].ours) continue;
        notifications[i].serial = 0;
        DBusMessageIter args;
        if (dbus.message_get_type(reply) == MESSAGE_METHOD_RETURN &&
            dbus.message_iter_init(reply, &args) && dbus.message_iter_get_arg_type(&args) == T_UINT32)
            dbus.message_iter_get_basic(&args, &notifications[i].server);
        return;
    }
}

static void notification_signal(DBusMessage *message) {
    const char *member = dbus.message_get_member(message);
    DBusMessageIter args;
    uint32_t server = 0;
    if (!member || !dbus.message_iter_init(message, &args) ||
        dbus.message_iter_get_arg_type(&args) != T_UINT32)
        return;
    dbus.message_iter_get_basic(&args, &server);
    for (size_t i = 0; i < 64; i++) {
        if (!server || notifications[i].server != server) continue;
        if (strcmp(member, "ActionInvoked") == 0) {
            const char *action = NULL;
            if (dbus.message_iter_next(&args) && dbus.message_iter_get_arg_type(&args) == T_STRING)
                dbus.message_iter_get_basic(&args, &action);
            if (action && strcmp(action, "default") == 0) {
                SilkWindowEvent event = silk_event(SILK_EVENT_NOTIFICATION_CLICKED, 0);
                event.code = notifications[i].ours;
                silk_queue_push(&event);
            }
        } else if (strcmp(member, "NotificationClosed") == 0) {
            notifications[i].ours = 0;
            notifications[i].server = 0;
        }
        return;
    }
}

// ---------------------------------------------------------------------------------------
// File dialogs: org.freedesktop.portal.FileChooser. The portal shows the desktop's own
// chooser, outside a Flatpak sandbox too, and answers with a Response signal on a request
// object whose path the caller can predict from its handle_token.

#define PORTAL_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define CHOOSER_INTERFACE "org.freedesktop.portal.FileChooser"

typedef struct PortalRequest {
    uint32_t dialog;
    char path[160];
} PortalRequest;

static PortalRequest portalRequests[16];
static uint32_t portalVersion; // 0 until asked, UINT32_MAX when there is no FileChooser
static uint32_t portalToken;

// The FileChooser version, or 0 without the portal. Asking starts the portal if the bus can
// activate it.
static uint32_t chooser_version(void) {
    if (portalVersion) return portalVersion == UINT32_MAX ? 0 : portalVersion;
    portalVersion = UINT32_MAX;
    if (!load()) return 0;
    DBusMessage *call = dbus.message_new_method_call(PORTAL_NAME, PORTAL_PATH,
                                                     "org.freedesktop.DBus.Properties", "Get");
    if (!call) return 0;
    DBusMessageIter args;
    dbus.message_iter_init_append(call, &args);
    append_string(&args, CHOOSER_INTERFACE);
    append_string(&args, "version");
    DBusError error;
    dbus.error_init(&error);
    DBusMessage *reply = dbus.connection_send_with_reply_and_block(bus, call, 5000, &error);
    dbus.error_free(&error);
    dbus.message_unref(call);
    if (!reply) return 0;
    DBusMessageIter values, variant;
    uint32_t version = 0;
    if (dbus.message_iter_init(reply, &values) && dbus.message_iter_get_arg_type(&values) == T_VARIANT) {
        dbus.message_iter_recurse(&values, &variant);
        if (dbus.message_iter_get_arg_type(&variant) == T_UINT32)
            dbus.message_iter_get_basic(&variant, &version);
    }
    dbus.message_unref(reply);
    if (!version) return 0;
    portalVersion = version;
    DBusError matchError;
    dbus.error_init(&matchError);
    dbus.bus_add_match(bus,
                       "type='signal',interface='org.freedesktop.portal.Request',member='Response'",
                       &matchError);
    dbus.error_free(&matchError);
    return version;
}

static void append_bytes_entry(DBusMessageIter *dict, const char *key, const char *text) {
    DBusMessageIter entry, variant, bytes;
    dbus.message_iter_open_container(dict, T_DICT_ENTRY, NULL, &entry);
    append_string(&entry, key);
    dbus.message_iter_open_container(&entry, T_VARIANT, "ay", &variant);
    dbus.message_iter_open_container(&variant, T_ARRAY, "y", &bytes);
    // A byte string with its final NUL, as the portal wants for paths.
    const uint8_t *data = (const uint8_t *)text;
    dbus.message_iter_append_fixed_array(&bytes, T_BYTE, &data, (int)strlen(text) + 1);
    dbus.message_iter_close_container(&variant, &bytes);
    dbus.message_iter_close_container(&entry, &variant);
    dbus.message_iter_close_container(dict, &entry);
}

// One filter named `label` that matches `extensions`, in either case.
static void append_filters(DBusMessageIter *dict, const char *label, char **extensions,
                           size_t count) {
    DBusMessageIter entry, variant, filters, filter, patterns, pattern;
    dbus.message_iter_open_container(dict, T_DICT_ENTRY, NULL, &entry);
    append_string(&entry, "filters");
    dbus.message_iter_open_container(&entry, T_VARIANT, "a(sa(us))", &variant);
    dbus.message_iter_open_container(&variant, T_ARRAY, "(sa(us))", &filters);
    dbus.message_iter_open_container(&filters, T_STRUCT, NULL, &filter);
    append_string(&filter, label);
    dbus.message_iter_open_container(&filter, T_ARRAY, "(us)", &patterns);
    for (size_t i = 0; i < count; i++) {
        // Glob patterns are case sensitive; [pP][nN][gG] matches both.
        size_t length = strlen(extensions[i]);
        char *glob = malloc(length * 4 + 3);
        if (!glob) abort();
        char *at = glob;
        *at++ = '*';
        *at++ = '.';
        for (size_t c = 0; c < length; c++) {
            char lower = extensions[i][c], upper = lower;
            if (lower >= 'a' && lower <= 'z') upper = (char)(lower - 32);
            else if (lower >= 'A' && lower <= 'Z') lower = (char)(lower + 32);
            if (lower == upper) {
                *at++ = lower;
            } else {
                *at++ = '[';
                *at++ = lower;
                *at++ = upper;
                *at++ = ']';
            }
        }
        *at = 0;
        uint32_t kind = 0;
        dbus.message_iter_open_container(&patterns, T_STRUCT, NULL, &pattern);
        dbus.message_iter_append_basic(&pattern, T_UINT32, &kind);
        append_string(&pattern, glob);
        dbus.message_iter_close_container(&patterns, &pattern);
        free(glob);
    }
    dbus.message_iter_close_container(&filter, &patterns);
    dbus.message_iter_close_container(&filters, &filter);
    dbus.message_iter_close_container(&variant, &filters);
    dbus.message_iter_close_container(&entry, &variant);
    dbus.message_iter_close_container(dict, &entry);
}

bool silk_portal_dialog(uint32_t dialog, bool save, const char *title, const char *directory,
                        const char *name, const char *label, char **extensions,
                        size_t extensionCount, bool directories, bool multiple,
                        const char *parent) {
    const char *disabled = getenv("SILK_WINDOW_PORTAL");
    if (disabled && strcmp(disabled, "0") == 0) return false;
    uint32_t version = chooser_version();
    // Directories need version 3; an older portal would let a file be chosen instead.
    if (!version || (directories && version < 3)) return false;
    PortalRequest *slot = NULL;
    for (size_t i = 0; i < 16 && !slot; i++)
        if (!portalRequests[i].dialog) slot = &portalRequests[i];
    if (!slot) return false;

    char token[32];
    snprintf(token, sizeof token, "silk%u_%u", (unsigned)getpid(), ++portalToken);
    // The request's path: the sender's unique name without ':' and with '.' as '_'.
    const char *unique = dbus.bus_get_unique_name(bus);
    char sender[96] = {0};
    size_t s = 0;
    for (const char *c = unique && unique[0] == ':' ? unique + 1 : unique; c && *c && s + 1 < sizeof sender; c++)
        sender[s++] = *c == '.' ? '_' : *c;
    snprintf(slot->path, sizeof slot->path, PORTAL_PATH "/request/%s/%s", sender, token);

    DBusMessage *call = dbus.message_new_method_call(PORTAL_NAME, PORTAL_PATH, CHOOSER_INTERFACE,
                                                     save ? "SaveFile" : "OpenFile");
    if (!call) return false;
    DBusMessageIter args, options;
    dbus.message_iter_init_append(call, &args);
    append_string(&args, parent);
    append_string(&args, title);
    dbus.message_iter_open_container(&args, T_ARRAY, "{sv}", &options);
    const char *tokenText = token;
    append_entry(&options, "handle_token", T_STRING, &tokenText);
    dbus_bool_t modal = 1;
    append_entry(&options, "modal", T_BOOLEAN, &modal);
    if (save) {
        if (name[0]) append_entry(&options, "current_name", T_STRING, &name);
    } else {
        dbus_bool_t many = multiple, folders = directories;
        append_entry(&options, "multiple", T_BOOLEAN, &many);
        if (directories) append_entry(&options, "directory", T_BOOLEAN, &folders);
    }
    if (directory[0] && (save || version >= 3)) append_bytes_entry(&options, "current_folder", directory);
    if (!directories && extensionCount) append_filters(&options, label, extensions, extensionCount);
    dbus.message_iter_close_container(&args, &options);

    DBusError error;
    dbus.error_init(&error);
    DBusMessage *reply = dbus.connection_send_with_reply_and_block(bus, call, 5000, &error);
    dbus.error_free(&error);
    dbus.message_unref(call);
    if (!reply) return false;
    // Portals before version 0.9 answer with a path of their own choosing.
    DBusMessageIter handle;
    if (dbus.message_iter_init(reply, &handle) && dbus.message_iter_get_arg_type(&handle) == T_OBJECT_PATH) {
        const char *path = NULL;
        dbus.message_iter_get_basic(&handle, &path);
        if (path) snprintf(slot->path, sizeof slot->path, "%s", path);
    }
    dbus.message_unref(reply);
    slot->dialog = dialog;
    return true;
}

static void portal_response(DBusMessage *message) {
    const char *path = dbus.message_get_path(message);
    PortalRequest *request = NULL;
    for (size_t i = 0; i < 16 && path && !request; i++)
        if (portalRequests[i].dialog && strcmp(portalRequests[i].path, path) == 0)
            request = &portalRequests[i];
    if (!request) return;
    DBusMessageIter args, results, entry, variant, uris;
    uint32_t response = 2;
    // The chosen files as a text/uri-list, for the shared percent-decoding.
    char *list = NULL;
    size_t length = 0;
    if (dbus.message_iter_init(message, &args) && dbus.message_iter_get_arg_type(&args) == T_UINT32) {
        dbus.message_iter_get_basic(&args, &response);
        if (response == 0 && dbus.message_iter_next(&args) &&
            dbus.message_iter_get_arg_type(&args) == T_ARRAY) {
            dbus.message_iter_recurse(&args, &results);
            while (dbus.message_iter_get_arg_type(&results) == T_DICT_ENTRY) {
                dbus.message_iter_recurse(&results, &entry);
                const char *key = NULL;
                dbus.message_iter_get_basic(&entry, &key);
                if (key && strcmp(key, "uris") == 0 && dbus.message_iter_next(&entry)) {
                    dbus.message_iter_recurse(&entry, &variant);
                    if (dbus.message_iter_get_arg_type(&variant) == T_ARRAY) {
                        dbus.message_iter_recurse(&variant, &uris);
                        while (dbus.message_iter_get_arg_type(&uris) == T_STRING) {
                            const char *uri = NULL;
                            dbus.message_iter_get_basic(&uris, &uri);
                            size_t add = strlen(uri);
                            char *grown = realloc(list, length + add + 3);
                            if (!grown) abort();
                            list = grown;
                            memcpy(list + length, uri, add);
                            length += add;
                            list[length++] = '\r';
                            list[length++] = '\n';
                            list[length] = 0;
                            dbus.message_iter_next(&uris);
                        }
                    }
                }
                dbus.message_iter_next(&results);
            }
        }
    }
    SilkText texts[SILK_MAX_TEXTS];
    size_t count = list ? silk_uri_list_paths((const uint8_t *)list, length, texts, SILK_MAX_TEXTS) : 0;
    SilkWindowEvent event = silk_event(SILK_EVENT_DIALOG_CLOSED, 0);
    event.code = request->dialog;
    event.count = (uint32_t)count;
    silk_queue_push_texts(&event, texts, count);
    silk_free_texts(texts, count);
    free(list);
    request->dialog = 0;
}

// ---------------------------------------------------------------------------------------
// The tray: a StatusNotifierItem at /StatusNotifierItem with its menu at /MenuBar.

#define ITEM_PATH "/StatusNotifierItem"
#define MENU_PATH "/MenuBar"
#define ITEM_INTERFACE "org.kde.StatusNotifierItem"
#define MENU_INTERFACE "com.canonical.dbusmenu"

static bool trayShown;
static bool trayNamed;
static char trayName[64];
static uint8_t *trayPixels; // ARGB32 in network byte order, as IconPixmap wants
static int32_t trayWidth, trayHeight;
static char *trayTooltip;
static uint32_t menuRevision = 1;

static void append_pixmaps(DBusMessageIter *iter) {
    DBusMessageIter pixmaps, pixmap, bytes;
    dbus.message_iter_open_container(iter, T_ARRAY, "(iiay)", &pixmaps);
    if (trayPixels) {
        dbus.message_iter_open_container(&pixmaps, T_STRUCT, NULL, &pixmap);
        dbus.message_iter_append_basic(&pixmap, T_INT32, &trayWidth);
        dbus.message_iter_append_basic(&pixmap, T_INT32, &trayHeight);
        dbus.message_iter_open_container(&pixmap, T_ARRAY, "y", &bytes);
        const uint8_t *data = trayPixels;
        dbus.message_iter_append_fixed_array(&bytes, T_BYTE, &data, trayWidth * trayHeight * 4);
        dbus.message_iter_close_container(&pixmap, &bytes);
        dbus.message_iter_close_container(&pixmaps, &pixmap);
    }
    dbus.message_iter_close_container(iter, &pixmaps);
}

// One StatusNotifierItem property as a variant, or false for a name it does not have.
static bool append_item_property(DBusMessageIter *iter, const char *name) {
    DBusMessageIter variant;
    const char *text = NULL;
    if (strcmp(name, "Category") == 0) text = "ApplicationStatus";
    else if (strcmp(name, "Id") == 0) text = program_invocation_short_name;
    else if (strcmp(name, "Title") == 0) text = trayTooltip ? trayTooltip : "";
    else if (strcmp(name, "Status") == 0) text = trayShown ? "Active" : "Passive";
    else if (strcmp(name, "IconName") == 0) text = "";
    if (text) {
        append_variant(iter, T_STRING, &text);
        return true;
    }
    if (strcmp(name, "ItemIsMenu") == 0) {
        dbus_bool_t value = silk_tray_count > 0;
        append_variant(iter, T_BOOLEAN, &value);
        return true;
    }
    if (strcmp(name, "WindowId") == 0) {
        int32_t value = 0;
        append_variant(iter, T_INT32, &value);
        return true;
    }
    if (strcmp(name, "Menu") == 0) {
        const char *path = silk_tray_count ? MENU_PATH : "/NO_DBUSMENU";
        append_variant(iter, T_OBJECT_PATH, &path);
        return true;
    }
    if (strcmp(name, "IconPixmap") == 0) {
        dbus.message_iter_open_container(iter, T_VARIANT, "a(iiay)", &variant);
        append_pixmaps(&variant);
        dbus.message_iter_close_container(iter, &variant);
        return true;
    }
    if (strcmp(name, "ToolTip") == 0) {
        DBusMessageIter tip;
        const char *empty = "";
        const char *title = trayTooltip ? trayTooltip : "";
        dbus.message_iter_open_container(iter, T_VARIANT, "(sa(iiay)ss)", &variant);
        dbus.message_iter_open_container(&variant, T_STRUCT, NULL, &tip);
        append_string(&tip, empty);
        append_pixmaps(&tip);
        append_string(&tip, title);
        append_string(&tip, empty);
        dbus.message_iter_close_container(&variant, &tip);
        dbus.message_iter_close_container(iter, &variant);
        return true;
    }
    return false;
}

static const char *const ITEM_PROPERTIES[] = {"Category", "Id", "Title", "Status", "IconName",
                                              "IconPixmap", "ToolTip", "ItemIsMenu", "Menu",
                                              "WindowId"};

static bool append_menu_property(DBusMessageIter *iter, const char *name) {
    if (strcmp(name, "Version") == 0) {
        uint32_t version = 3;
        append_variant(iter, T_UINT32, &version);
    } else if (strcmp(name, "TextDirection") == 0) {
        const char *direction = "ltr";
        append_variant(iter, T_STRING, &direction);
    } else if (strcmp(name, "Status") == 0) {
        const char *status = "normal";
        append_variant(iter, T_STRING, &status);
    } else if (strcmp(name, "IconThemePath") == 0) {
        DBusMessageIter variant, paths;
        dbus.message_iter_open_container(iter, T_VARIANT, "as", &variant);
        dbus.message_iter_open_container(&variant, T_ARRAY, "s", &paths);
        dbus.message_iter_close_container(&variant, &paths);
        dbus.message_iter_close_container(iter, &variant);
    } else {
        return false;
    }
    return true;
}

static const char *const MENU_PROPERTIES[] = {"Version", "TextDirection", "Status",
                                              "IconThemePath"};

// Properties.Get and GetAll for either object.
static void answer_properties(DBusMessage *call, bool item) {
    const char *member = dbus.message_get_member(call);
    DBusMessageIter args, reply;
    const char *interface = NULL, *name = NULL;
    if (dbus.message_iter_init(call, &args) && dbus.message_iter_get_arg_type(&args) == T_STRING) {
        dbus.message_iter_get_basic(&args, &interface);
        if (dbus.message_iter_next(&args) && dbus.message_iter_get_arg_type(&args) == T_STRING)
            dbus.message_iter_get_basic(&args, &name);
    }
    DBusMessage *answer = dbus.message_new_method_return(call);
    if (!answer) return;
    dbus.message_iter_init_append(answer, &reply);
    if (strcmp(member, "GetAll") == 0) {
        DBusMessageIter dict, entry;
        dbus.message_iter_open_container(&reply, T_ARRAY, "{sv}", &dict);
        const char *const *names = item ? ITEM_PROPERTIES : MENU_PROPERTIES;
        size_t count = item ? sizeof ITEM_PROPERTIES / sizeof *ITEM_PROPERTIES
                            : sizeof MENU_PROPERTIES / sizeof *MENU_PROPERTIES;
        for (size_t i = 0; i < count; i++) {
            dbus.message_iter_open_container(&dict, T_DICT_ENTRY, NULL, &entry);
            append_string(&entry, names[i]);
            if (item) append_item_property(&entry, names[i]);
            else append_menu_property(&entry, names[i]);
            dbus.message_iter_close_container(&dict, &entry);
        }
        dbus.message_iter_close_container(&reply, &dict);
    } else if (strcmp(member, "Get") == 0 && name &&
               (item ? append_item_property(&reply, name) : append_menu_property(&reply, name))) {
    } else {
        dbus.message_unref(answer);
        answer = dbus.message_new_error(call, "org.freedesktop.DBus.Error.UnknownProperty",
                                        "No such property");
    }
    send_and_unref(answer);
}

// Menu items are numbered by their entry's index + 1; 0 is the root.

// The index past the entry that ends the submenu opened at `begin`.
static size_t skip_submenu(size_t begin) {
    int depth = 0;
    for (size_t i = begin; i < silk_tray_count; i++) {
        if (silk_tray_entries[i].type == SILK_MENU_BEGIN) depth++;
        if (silk_tray_entries[i].type == SILK_MENU_END && --depth == 0) return i + 1;
    }
    return silk_tray_count;
}

static bool shown_entry(const SilkMenuEntry *entry) {
    if (entry->type == SILK_MENU_SYSTEM) return entry->item == SILK_ITEM_QUIT;
    return entry->type != SILK_MENU_END;
}

static void append_item_properties(DBusMessageIter *iter, int32_t id) {
    DBusMessageIter dict;
    dbus.message_iter_open_container(iter, T_ARRAY, "{sv}", &dict);
    if (id > 0 && (size_t)id <= silk_tray_count) {
        const SilkMenuEntry *entry = &silk_tray_entries[id - 1];
        if (entry->type == SILK_MENU_SEPARATOR) {
            const char *separator = "separator";
            append_entry(&dict, "type", T_STRING, &separator);
        } else {
            char *label = entry->type == SILK_MENU_SYSTEM
                              ? strdup("Quit")
                              : silk_strndup(entry->title, entry->titleLength);
            const char *text = label ? label : "";
            append_entry(&dict, "label", T_STRING, &text);
            free(label);
        }
        if (entry->type == SILK_MENU_ACTION) {
            dbus_bool_t enabled = entry->enabled != 0;
            append_entry(&dict, "enabled", T_BOOLEAN, &enabled);
            if (entry->checked) {
                const char *toggle = "checkmark";
                int32_t state = 1;
                append_entry(&dict, "toggle-type", T_STRING, &toggle);
                append_entry(&dict, "toggle-state", T_INT32, &state);
            }
        }
        if (entry->type == SILK_MENU_BEGIN) {
            const char *display = "submenu";
            append_entry(&dict, "children-display", T_STRING, &display);
        }
    } else if (id == 0) {
        const char *display = "submenu";
        append_entry(&dict, "children-display", T_STRING, &display);
    }
    dbus.message_iter_close_container(iter, &dict);
}

// One (ia{sv}av) layout node: the item `id` and its children down to `depth` levels.
static void append_layout(DBusMessageIter *iter, int32_t id, int32_t depth) {
    DBusMessageIter node, children;
    dbus.message_iter_open_container(iter, T_STRUCT, NULL, &node);
    dbus.message_iter_append_basic(&node, T_INT32, &id);
    append_item_properties(&node, id);
    dbus.message_iter_open_container(&node, T_ARRAY, "v", &children);
    bool container = id == 0 || ((size_t)id <= silk_tray_count &&
                                 silk_tray_entries[id - 1].type == SILK_MENU_BEGIN);
    if (container && depth != 0) {
        size_t i = id == 0 ? 0 : (size_t)id;
        size_t end = id == 0 ? silk_tray_count : skip_submenu((size_t)id - 1) - 1;
        while (i < end) {
            const SilkMenuEntry *entry = &silk_tray_entries[i];
            if (shown_entry(entry)) {
                DBusMessageIter variant;
                dbus.message_iter_open_container(&children, T_VARIANT, "(ia{sv}av)", &variant);
                append_layout(&variant, (int32_t)i + 1, depth < 0 ? -1 : depth - 1);
                dbus.message_iter_close_container(&children, &variant);
            }
            i = entry->type == SILK_MENU_BEGIN ? skip_submenu(i) : i + 1;
        }
    }
    dbus.message_iter_close_container(&node, &children);
    dbus.message_iter_close_container(iter, &node);
}

static void menu_clicked(int32_t id) {
    if (id <= 0 || (size_t)id > silk_tray_count) return;
    const SilkMenuEntry *entry = &silk_tray_entries[id - 1];
    if (entry->type == SILK_MENU_ACTION && entry->enabled) {
        SilkWindowEvent event = silk_event(SILK_EVENT_MENU_ACTION, 0);
        event.code = entry->action;
        silk_queue_push(&event);
    } else if (entry->type == SILK_MENU_SYSTEM && entry->item == SILK_ITEM_QUIT) {
        SilkWindowEvent event = silk_event(SILK_EVENT_QUIT_REQUESTED, 0);
        silk_queue_push(&event);
    }
}

// The id and event name of one dbusmenu event, from an Event call or an EventGroup entry.
static void menu_event(DBusMessageIter *fields) {
    int32_t id = 0;
    const char *name = NULL;
    if (dbus.message_iter_get_arg_type(fields) != T_INT32) return;
    dbus.message_iter_get_basic(fields, &id);
    if (!dbus.message_iter_next(fields) || dbus.message_iter_get_arg_type(fields) != T_STRING) return;
    dbus.message_iter_get_basic(fields, &name);
    if (strcmp(name, "clicked") == 0) menu_clicked(id);
}

static void answer_menu(DBusMessage *call) {
    const char *member = dbus.message_get_member(call);
    DBusMessageIter args, reply;
    bool hasArgs = dbus.message_iter_init(call, &args);
    DBusMessage *answer = dbus.message_new_method_return(call);
    if (!answer) return;
    dbus.message_iter_init_append(answer, &reply);
    if (strcmp(member, "GetLayout") == 0) {
        int32_t parent = 0, depth = -1;
        if (hasArgs && dbus.message_iter_get_arg_type(&args) == T_INT32) {
            dbus.message_iter_get_basic(&args, &parent);
            if (dbus.message_iter_next(&args) && dbus.message_iter_get_arg_type(&args) == T_INT32)
                dbus.message_iter_get_basic(&args, &depth);
        }
        dbus.message_iter_append_basic(&reply, T_UINT32, &menuRevision);
        append_layout(&reply, parent, depth);
    } else if (strcmp(member, "GetGroupProperties") == 0) {
        DBusMessageIter ids, items, item;
        dbus.message_iter_open_container(&reply, T_ARRAY, "(ia{sv})", &items);
        if (hasArgs && dbus.message_iter_get_arg_type(&args) == T_ARRAY) {
            dbus.message_iter_recurse(&args, &ids);
            while (dbus.message_iter_get_arg_type(&ids) == T_INT32) {
                int32_t id = 0;
                dbus.message_iter_get_basic(&ids, &id);
                dbus.message_iter_open_container(&items, T_STRUCT, NULL, &item);
                dbus.message_iter_append_basic(&item, T_INT32, &id);
                append_item_properties(&item, id);
                dbus.message_iter_close_container(&items, &item);
                dbus.message_iter_next(&ids);
            }
        }
        dbus.message_iter_close_container(&reply, &items);
    } else if (strcmp(member, "GetProperty") == 0) {
        // Rarely used; every property is in GetLayout.
        const char *empty = "";
        append_variant(&reply, T_STRING, &empty);
    } else if (strcmp(member, "Event") == 0) {
        if (hasArgs) menu_event(&args);
    } else if (strcmp(member, "EventGroup") == 0) {
        DBusMessageIter events, event, errors;
        if (hasArgs && dbus.message_iter_get_arg_type(&args) == T_ARRAY) {
            dbus.message_iter_recurse(&args, &events);
            while (dbus.message_iter_get_arg_type(&events) == T_STRUCT) {
                dbus.message_iter_recurse(&events, &event);
                menu_event(&event);
                dbus.message_iter_next(&events);
            }
        }
        dbus.message_iter_open_container(&reply, T_ARRAY, "i", &errors);
        dbus.message_iter_close_container(&reply, &errors);
    } else if (strcmp(member, "AboutToShow") == 0) {
        dbus_bool_t update = 0;
        dbus.message_iter_append_basic(&reply, T_BOOLEAN, &update);
    } else if (strcmp(member, "AboutToShowGroup") == 0) {
        DBusMessageIter updates, errors;
        dbus.message_iter_open_container(&reply, T_ARRAY, "i", &updates);
        dbus.message_iter_close_container(&reply, &updates);
        dbus.message_iter_open_container(&reply, T_ARRAY, "i", &errors);
        dbus.message_iter_close_container(&reply, &errors);
    } else {
        dbus.message_unref(answer);
        answer = dbus.message_new_error(call, "org.freedesktop.DBus.Error.UnknownMethod",
                                        "No such method");
    }
    send_and_unref(answer);
}

static void answer_item(DBusMessage *call) {
    const char *member = dbus.message_get_member(call);
    uint32_t button = SILK_BUTTON_OTHER;
    if (strcmp(member, "Activate") == 0) button = SILK_BUTTON_LEFT;
    else if (strcmp(member, "SecondaryActivate") == 0) button = SILK_BUTTON_MIDDLE;
    else if (strcmp(member, "ContextMenu") == 0) button = SILK_BUTTON_RIGHT;
    if (button != SILK_BUTTON_OTHER && trayShown) {
        SilkWindowEvent event = silk_event(SILK_EVENT_TRAY_CLICKED, 0);
        event.code = button;
        silk_queue_push(&event);
    }
    send_and_unref(dbus.message_new_method_return(call));
}

static void signal_item(const char *name) {
    send_and_unref(dbus.message_new_signal(ITEM_PATH, ITEM_INTERFACE, name));
}

static void signal_layout(void) {
    DBusMessage *message = dbus.message_new_signal(MENU_PATH, MENU_INTERFACE, "LayoutUpdated");
    if (!message) return;
    DBusMessageIter args;
    int32_t parent = 0;
    dbus.message_iter_init_append(message, &args);
    dbus.message_iter_append_basic(&args, T_UINT32, &menuRevision);
    dbus.message_iter_append_basic(&args, T_INT32, &parent);
    send_and_unref(message);
}

static bool watcher_register(void) {
    DBusMessage *call = dbus.message_new_method_call(
        "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
        "RegisterStatusNotifierItem");
    if (!call) return false;
    DBusMessageIter args;
    dbus.message_iter_init_append(call, &args);
    append_string(&args, trayName);
    DBusError error;
    dbus.error_init(&error);
    DBusMessage *reply = dbus.connection_send_with_reply_and_block(bus, call, 1000, &error);
    dbus.message_unref(call);
    dbus.error_free(&error);
    if (!reply) return false;
    dbus.message_unref(reply);
    return true;
}

uint8_t silk_window_tray_commit(const uint8_t *pixels, uint32_t width, uint32_t height,
                                const uint8_t *tooltip, size_t tooltipLength, uint32_t flags) {
    (void)flags;
    silk_tray_commit_pending();
    if (!load()) return 0;
    free(trayPixels);
    trayPixels = NULL;
    trayWidth = (int32_t)width;
    trayHeight = (int32_t)height;
    size_t count = (size_t)width * height;
    if (count) {
        trayPixels = malloc(count * 4);
        if (!trayPixels) abort();
        for (size_t i = 0; i < count; i++) {
            trayPixels[i * 4 + 0] = pixels[i * 4 + 3];
            trayPixels[i * 4 + 1] = pixels[i * 4 + 0];
            trayPixels[i * 4 + 2] = pixels[i * 4 + 1];
            trayPixels[i * 4 + 3] = pixels[i * 4 + 2];
        }
    }
    free(trayTooltip);
    trayTooltip = silk_strndup(tooltip, tooltipLength);
    menuRevision++;
    if (trayShown) {
        signal_item("NewIcon");
        signal_item("NewToolTip");
        signal_item("NewTitle");
        signal_layout();
        dbus.connection_flush(bus);
        return 1;
    }
    if (!trayNamed) {
        snprintf(trayName, sizeof trayName, "org.kde.StatusNotifierItem-%d-1", (int)getpid());
        DBusError error;
        dbus.error_init(&error);
        int result = dbus.bus_request_name(bus, trayName, NAME_DO_NOT_QUEUE, &error);
        dbus.error_free(&error);
        // 1 primary owner, 4 already the owner.
        if (result != 1 && result != 4) return 0;
        trayNamed = true;
    }
    trayShown = watcher_register();
    if (trayShown) {
        // After tray_remove the item was passive; say it is back.
        DBusMessage *message = dbus.message_new_signal(ITEM_PATH, ITEM_INTERFACE, "NewStatus");
        if (message) {
            DBusMessageIter args;
            dbus.message_iter_init_append(message, &args);
            append_string(&args, "Active");
            send_and_unref(message);
        }
        dbus.connection_flush(bus);
    }
    return trayShown ? 1 : 0;
}

void silk_window_tray_remove(void) {
    if (!trayShown) return;
    // Watchers drop items whose name goes away; the name is kept, so mark it passive instead.
    trayShown = false;
    DBusMessage *message = dbus.message_new_signal(ITEM_PATH, ITEM_INTERFACE, "NewStatus");
    if (message) {
        DBusMessageIter args;
        dbus.message_iter_init_append(message, &args);
        append_string(&args, "Passive");
        send_and_unref(message);
    }
    dbus.connection_flush(bus);
}

// ---------------------------------------------------------------------------------------
// The launcher entry: a count and progress on the app's icon in the dock.

static int64_t launcherCount = -1; // negative: none
static double launcherProgress = -1.0;

// The desktop file the dock knows the app by: SILK_DESKTOP_ID (with or without .desktop),
// else the program's name.
static void launcher_uri(char *uri, size_t capacity) {
    const char *id = getenv("SILK_DESKTOP_ID");
    if (!id || !*id) id = program_invocation_short_name;
    size_t length = strlen(id);
    bool suffixed = length > 8 && strcmp(id + length - 8, ".desktop") == 0;
    snprintf(uri, capacity, "application://%s%s", id, suffixed ? "" : ".desktop");
}

static uint8_t launcher_update(void) {
    if (!load()) return 0;
    DBusMessage *message = dbus.message_new_signal("/com/canonical/unity/launcherentry/silk",
                                                   "com.canonical.Unity.LauncherEntry", "Update");
    if (!message) return 0;
    char uri[512];
    launcher_uri(uri, sizeof uri);
    DBusMessageIter args, properties;
    dbus.message_iter_init_append(message, &args);
    append_string(&args, uri);
    dbus.message_iter_open_container(&args, T_ARRAY, "{sv}", &properties);
    int64_t count = launcherCount < 0 ? 0 : launcherCount;
    dbus_bool_t countShown = launcherCount >= 0;
    double progress = launcherProgress < 0 ? 0 : launcherProgress;
    dbus_bool_t progressShown = launcherProgress >= 0;
    append_entry(&properties, "count", T_INT64, &count);
    append_entry(&properties, "count-visible", T_BOOLEAN, &countShown);
    append_entry(&properties, "progress", T_DOUBLE, &progress);
    append_entry(&properties, "progress-visible", T_BOOLEAN, &progressShown);
    dbus.message_iter_close_container(&args, &properties);
    send_and_unref(message);
    dbus.connection_flush(bus);
    return 1;
}

uint8_t silk_window_set_badge(const uint8_t *text, size_t length) {
    // Docks show counts only: other text shows no badge.
    int64_t count = length ? 0 : -1;
    for (size_t i = 0; i < length; i++) {
        if (text[i] < '0' || text[i] > '9' || count > 1000000000) {
            count = -1;
            break;
        }
        count = count * 10 + (text[i] - '0');
    }
    launcherCount = count;
    return launcher_update();
}

uint8_t silk_window_set_progress(double fraction) {
    launcherProgress = fraction < 0 ? -1.0 : fraction > 1 ? 1.0 : fraction;
    return launcher_update();
}

// ---------------------------------------------------------------------------------------
// URLs and the file manager.

// Runs `xdg-open target` detached: the child forks again and exits, so nothing is left to reap.
static uint8_t xdg_open(const char *target) {
    pid_t child = fork();
    if (child < 0) return 0;
    if (child == 0) {
        if (fork() == 0) {
            execlp("xdg-open", "xdg-open", target, (char *)NULL);
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 1 : 0;
}

uint8_t silk_window_open_url(const uint8_t *url, size_t length) {
    if (!length) return 0;
    char *target = silk_strndup(url, length);
    if (!target) return 0;
    uint8_t opened = strchr(target, ':') ? xdg_open(target) : 0;
    free(target);
    return opened;
}

// org.freedesktop.FileManager1 (Nautilus, Dolphin, Nemo, Thunar) selects the item; without one
// the folder holding it opens.
uint8_t silk_window_reveal_path(const uint8_t *path, size_t length) {
    if (!length || path[0] != '/') return 0;
    char *target = silk_strndup(path, length);
    if (!target) return 0;
    uint8_t shown = 0;
    if (load()) {
        DBusMessage *call = dbus.message_new_method_call(
            "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
            "org.freedesktop.FileManager1", "ShowItems");
        if (call) {
            DBusMessageIter args, uris;
            dbus.message_iter_init_append(call, &args);
            dbus.message_iter_open_container(&args, T_ARRAY, "s", &uris);
            size_t size = strlen(target) * 3 + 8;
            char *uri = malloc(size);
            if (uri) {
                // file:// with every byte outside the unreserved set and '/' escaped.
                size_t at = (size_t)snprintf(uri, size, "file://");
                for (const unsigned char *c = (const unsigned char *)target; *c; c++) {
                    if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
                        (*c >= '0' && *c <= '9') || strchr("/-._~", *c)) {
                        uri[at++] = (char)*c;
                    } else {
                        at += (size_t)snprintf(uri + at, size - at, "%%%02X", *c);
                    }
                }
                uri[at] = 0;
                append_string(&uris, uri);
                free(uri);
            }
            dbus.message_iter_close_container(&args, &uris);
            append_string(&args, "");
            DBusError error;
            dbus.error_init(&error);
            DBusMessage *reply = dbus.connection_send_with_reply_and_block(bus, call, 2000, &error);
            dbus.message_unref(call);
            if (reply) {
                shown = dbus.message_get_type(reply) == MESSAGE_METHOD_RETURN;
                dbus.message_unref(reply);
            }
            dbus.error_free(&error);
        }
    }
    if (!shown) {
        char *slash = strrchr(target, '/');
        if (slash) {
            if (slash == target) slash[1] = 0;
            else *slash = 0;
            shown = xdg_open(target);
        }
    }
    free(target);
    return shown;
}

// Dragging files out needs a toolkit's drag source (XDND or wl_data_source), which this
// backend does not have.
uint8_t silk_window_start_file_drag(uint32_t window, const uint8_t *paths, size_t length) {
    (void)window;
    (void)paths;
    (void)length;
    return 0;
}

// A Linux window has no toolkit to draw a context menu: the app draws its own.
uint8_t silk_window_context_menu_commit(uint32_t window, double x, double y) {
    (void)window;
    (void)x;
    (void)y;
    silk_context_commit_pending();
    return 0;
}

// ---------------------------------------------------------------------------------------

// For hosts that introspect before they call.
static const char ITEM_XML[] =
    "<node><interface name=\"org.kde.StatusNotifierItem\">"
    "<property name=\"Category\" type=\"s\" access=\"read\"/>"
    "<property name=\"Id\" type=\"s\" access=\"read\"/>"
    "<property name=\"Title\" type=\"s\" access=\"read\"/>"
    "<property name=\"Status\" type=\"s\" access=\"read\"/>"
    "<property name=\"WindowId\" type=\"i\" access=\"read\"/>"
    "<property name=\"IconName\" type=\"s\" access=\"read\"/>"
    "<property name=\"IconPixmap\" type=\"a(iiay)\" access=\"read\"/>"
    "<property name=\"ToolTip\" type=\"(sa(iiay)ss)\" access=\"read\"/>"
    "<property name=\"ItemIsMenu\" type=\"b\" access=\"read\"/>"
    "<property name=\"Menu\" type=\"o\" access=\"read\"/>"
    "<method name=\"Activate\"><arg type=\"i\" direction=\"in\"/><arg type=\"i\" direction=\"in\"/></method>"
    "<method name=\"SecondaryActivate\"><arg type=\"i\" direction=\"in\"/><arg type=\"i\" direction=\"in\"/></method>"
    "<method name=\"ContextMenu\"><arg type=\"i\" direction=\"in\"/><arg type=\"i\" direction=\"in\"/></method>"
    "<method name=\"Scroll\"><arg type=\"i\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/></method>"
    "<signal name=\"NewTitle\"/><signal name=\"NewIcon\"/><signal name=\"NewToolTip\"/>"
    "<signal name=\"NewStatus\"><arg type=\"s\"/></signal>"
    "</interface></node>";

static const char MENU_XML[] =
    "<node><interface name=\"com.canonical.dbusmenu\">"
    "<property name=\"Version\" type=\"u\" access=\"read\"/>"
    "<property name=\"TextDirection\" type=\"s\" access=\"read\"/>"
    "<property name=\"Status\" type=\"s\" access=\"read\"/>"
    "<property name=\"IconThemePath\" type=\"as\" access=\"read\"/>"
    "<method name=\"GetLayout\"><arg type=\"i\" direction=\"in\"/><arg type=\"i\" direction=\"in\"/>"
    "<arg type=\"as\" direction=\"in\"/><arg type=\"u\" direction=\"out\"/>"
    "<arg type=\"(ia{sv}av)\" direction=\"out\"/></method>"
    "<method name=\"GetGroupProperties\"><arg type=\"ai\" direction=\"in\"/><arg type=\"as\" direction=\"in\"/>"
    "<arg type=\"a(ia{sv})\" direction=\"out\"/></method>"
    "<method name=\"GetProperty\"><arg type=\"i\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/>"
    "<arg type=\"v\" direction=\"out\"/></method>"
    "<method name=\"Event\"><arg type=\"i\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/>"
    "<arg type=\"v\" direction=\"in\"/><arg type=\"u\" direction=\"in\"/></method>"
    "<method name=\"EventGroup\"><arg type=\"a(isvu)\" direction=\"in\"/><arg type=\"ai\" direction=\"out\"/></method>"
    "<method name=\"AboutToShow\"><arg type=\"i\" direction=\"in\"/><arg type=\"b\" direction=\"out\"/></method>"
    "<method name=\"AboutToShowGroup\"><arg type=\"ai\" direction=\"in\"/><arg type=\"ai\" direction=\"out\"/>"
    "<arg type=\"ai\" direction=\"out\"/></method>"
    "<signal name=\"LayoutUpdated\"><arg type=\"u\"/><arg type=\"i\"/></signal>"
    "</interface></node>";

static void handle(DBusMessage *message) {
    int type = dbus.message_get_type(message);
    if (type == MESSAGE_METHOD_RETURN || type == MESSAGE_ERROR) {
        notify_replied(message);
        return;
    }
    const char *interface = dbus.message_get_interface(message);
    const char *path = dbus.message_get_path(message);
    if (type == MESSAGE_SIGNAL) {
        if (interface && strcmp(interface, "org.freedesktop.Notifications") == 0)
            notification_signal(message);
        else if (interface && strcmp(interface, "org.freedesktop.portal.Request") == 0)
            portal_response(message);
        return;
    }
    if (type != MESSAGE_METHOD_CALL || !path) return;
    bool item = strcmp(path, ITEM_PATH) == 0, menu = strcmp(path, MENU_PATH) == 0;
    if (!item && !menu) {
        send_and_unref(dbus.message_new_error(message, "org.freedesktop.DBus.Error.UnknownObject",
                                              "No such object"));
        return;
    }
    if (interface && strcmp(interface, "org.freedesktop.DBus.Introspectable") == 0) {
        DBusMessage *answer = dbus.message_new_method_return(message);
        if (!answer) return;
        DBusMessageIter args;
        dbus.message_iter_init_append(answer, &args);
        append_string(&args, item ? ITEM_XML : MENU_XML);
        send_and_unref(answer);
    } else if (interface && strcmp(interface, "org.freedesktop.DBus.Properties") == 0) {
        answer_properties(message, item);
    } else if (item) {
        answer_item(message);
    } else {
        answer_menu(message);
    }
}

void silk_dbus_dispatch(void) {
    if (!bus) return;
    dbus.connection_read_write(bus, 0);
    DBusMessage *message;
    while ((message = dbus.connection_pop_message(bus))) {
        handle(message);
        dbus.message_unref(message);
    }
    dbus.connection_flush(bus);
}
