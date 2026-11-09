// Pieces the Windows and Linux bridges share: the event queue with its strings, the strings
// of the event last returned, menu shortcuts for backends that match them themselves, and
// UTF-8 helpers. Single-threaded: everything runs on the platform's thread.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../bridge.h"

enum { SILK_MAX_TEXTS = 64 };

typedef struct SilkText {
    uint8_t *bytes;
    size_t length;
} SilkText;

// Queues an event with no strings.
void silk_queue_push(const SilkWindowEvent *event);
// Queues an event with `count` strings, copied.
void silk_queue_push_texts(const SilkWindowEvent *event, const SilkText *texts, size_t count);
// Queues an event with one string, copied.
void silk_queue_push_text(const SilkWindowEvent *event, const uint8_t *text, size_t length);
bool silk_queue_empty(void);
// Moves the oldest event to `event` and its strings to the current-event strings that
// silk_window_event_text reads. Returns false when the queue is empty.
bool silk_queue_pop(SilkWindowEvent *event);
// Drops queued events of `window` (after it closes).
void silk_queue_forget_window(uint32_t window);

SilkWindowEvent silk_event(uint32_t kind, uint32_t window);

// Monotonic seconds.
double silk_now(void);

// Menu shortcuts. silk_menu_* build the list between begin_bar and commit; backends with a
// native menu bar also read the entries.
typedef struct SilkMenuEntry {
    uint32_t type; // 1 begin, 2 end, 3 action, 4 separator, 5 system
    uint8_t *title;
    size_t titleLength;
    uint32_t action;
    uint32_t keyKind;
    uint32_t keyValue;
    uint32_t modifiers;
    uint8_t enabled;
    uint8_t checked;
    uint32_t item;
} SilkMenuEntry;

enum {
    SILK_MENU_BEGIN = 1,
    SILK_MENU_END = 2,
    SILK_MENU_ACTION = 3,
    SILK_MENU_SEPARATOR = 4,
    SILK_MENU_SYSTEM = 5,
};

// menu.SystemItem discriminants.
enum {
    SILK_ITEM_ABOUT = 0,
    SILK_ITEM_SERVICES = 1,
    SILK_ITEM_HIDE = 2,
    SILK_ITEM_HIDE_OTHERS = 3,
    SILK_ITEM_SHOW_ALL = 4,
    SILK_ITEM_QUIT = 5,
    SILK_ITEM_CLOSE_WINDOW = 6,
    SILK_ITEM_MINIMIZE = 7,
    SILK_ITEM_ZOOM = 8,
    SILK_ITEM_TOGGLE_FULL_SCREEN = 9,
    SILK_ITEM_BRING_ALL_TO_FRONT = 10,
};

// The committed menu bar.
extern SilkMenuEntry *silk_menu_entries;
extern size_t silk_menu_count;

void silk_menu_reset_pending(void);
void silk_menu_add(SilkMenuEntry entry, const uint8_t *title, size_t titleLength);
void silk_menu_commit_pending(void);

// What a matched shortcut does.
typedef struct SilkShortcutMatch {
    uint32_t type;   // 0 none, SILK_MENU_ACTION or SILK_MENU_SYSTEM
    uint32_t action; // action id, or SystemItem
} SilkShortcutMatch;

// Matches a key press against the committed menu bar's shortcuts. `character` is the
// lowercase Unicode scalar the key types without modifiers (0 for none) and `named` the
// NamedKey it produces (0 for none). Caps lock is ignored.
SilkShortcutMatch silk_menu_match(uint32_t character, uint32_t named, uint32_t modifiers);

// UTF-8.
size_t silk_utf8_encode(uint32_t scalar, uint8_t out[4]);
// The first scalar of `text`, or 0.
uint32_t silk_utf8_first(const uint8_t *text, size_t length);
uint32_t silk_ascii_lower(uint32_t scalar);
// Whether `text` holds only control characters (and so types nothing).
bool silk_utf8_is_control(const uint8_t *text, size_t length);

// Dialog results are delivered by id.
uint32_t silk_next_dialog_id(void);

// "png,jpg" split into a NUL-terminated array of NUL-terminated strings; free with
// silk_free_extensions.
char **silk_split_extensions(const uint8_t *extensions, size_t length, size_t *count);
void silk_free_extensions(char **list);
char *silk_strndup(const uint8_t *bytes, size_t length);
