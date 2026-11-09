#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "core.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct Queued {
    SilkWindowEvent event;
    SilkText *texts;
    size_t count;
} Queued;

static Queued *queue;
static size_t queueHead, queueLength, queueCapacity;
static SilkText currentTexts[SILK_MAX_TEXTS];
static size_t currentCount;
static uint32_t nextDialog = 1;

SilkWindowEvent silk_event(uint32_t kind, uint32_t window) {
    SilkWindowEvent event;
    memset(&event, 0, sizeof event);
    event.kind = kind;
    event.window = window;
    return event;
}

static void grow(void) {
    if (queueLength < queueCapacity) return;
    size_t capacity = queueCapacity ? queueCapacity * 2 : 64;
    Queued *items = malloc(capacity * sizeof *items);
    if (!items) abort();
    for (size_t i = 0; i < queueLength; i++) items[i] = queue[(queueHead + i) % queueCapacity];
    free(queue);
    queue = items;
    queueHead = 0;
    queueCapacity = capacity;
}

void silk_queue_push_texts(const SilkWindowEvent *event, const SilkText *texts, size_t count) {
    grow();
    Queued *slot = &queue[(queueHead + queueLength) % queueCapacity];
    slot->event = *event;
    slot->texts = NULL;
    slot->count = 0;
    if (count > SILK_MAX_TEXTS) count = SILK_MAX_TEXTS;
    if (count) {
        slot->texts = calloc(count, sizeof *slot->texts);
        if (!slot->texts) abort();
        for (size_t i = 0; i < count; i++) {
            slot->texts[i].length = texts[i].length;
            slot->texts[i].bytes = malloc(texts[i].length ? texts[i].length : 1);
            if (!slot->texts[i].bytes) abort();
            if (texts[i].length) memcpy(slot->texts[i].bytes, texts[i].bytes, texts[i].length);
        }
        slot->count = count;
    }
    queueLength++;
}

void silk_queue_push(const SilkWindowEvent *event) { silk_queue_push_texts(event, NULL, 0); }

void silk_queue_push_text(const SilkWindowEvent *event, const uint8_t *text, size_t length) {
    SilkText one = {(uint8_t *)text, length};
    silk_queue_push_texts(event, &one, 1);
}

bool silk_queue_empty(void) { return queueLength == 0; }

static void clear_current(void) {
    for (size_t i = 0; i < currentCount; i++) free(currentTexts[i].bytes);
    currentCount = 0;
}

bool silk_queue_pop(SilkWindowEvent *event) {
    clear_current();
    if (queueLength == 0) return false;
    Queued *slot = &queue[queueHead];
    *event = slot->event;
    for (size_t i = 0; i < slot->count; i++) currentTexts[i] = slot->texts[i];
    currentCount = slot->count;
    free(slot->texts);
    queueHead = (queueHead + 1) % queueCapacity;
    queueLength--;
    return true;
}

void silk_queue_forget_window(uint32_t window) {
    if (!window) return;
    size_t kept = 0;
    for (size_t i = 0; i < queueLength; i++) {
        Queued item = queue[(queueHead + i) % queueCapacity];
        if (item.event.window == window && item.event.kind != SILK_EVENT_DIALOG_CLOSED &&
            item.event.kind != SILK_EVENT_MENU_ACTION) {
            for (size_t t = 0; t < item.count; t++) free(item.texts[t].bytes);
            free(item.texts);
            continue;
        }
        queue[(queueHead + kept) % queueCapacity] = item;
        kept++;
    }
    queueLength = kept;
}

size_t silk_window_event_text(size_t index, uint8_t *destination, size_t capacity) {
    if (index >= currentCount) return 0;
    size_t length = currentTexts[index].length;
    if (destination && capacity) memcpy(destination, currentTexts[index].bytes,
                                        length < capacity ? length : capacity);
    return length;
}

double silk_now(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
#endif
}

// ---------------------------------------------------------------------------------------
// Menus.

SilkMenuEntry *silk_menu_entries;
size_t silk_menu_count;
static SilkMenuEntry *pending;
static size_t pendingCount, pendingCapacity;

static void free_entries(SilkMenuEntry *entries, size_t count) {
    for (size_t i = 0; i < count; i++) free(entries[i].title);
    free(entries);
}

void silk_menu_reset_pending(void) {
    free_entries(pending, pendingCount);
    pending = NULL;
    pendingCount = pendingCapacity = 0;
}

void silk_menu_add(SilkMenuEntry entry, const uint8_t *title, size_t titleLength) {
    if (pendingCount == pendingCapacity) {
        size_t capacity = pendingCapacity ? pendingCapacity * 2 : 32;
        SilkMenuEntry *entries = realloc(pending, capacity * sizeof *entries);
        if (!entries) abort();
        pending = entries;
        pendingCapacity = capacity;
    }
    entry.title = NULL;
    entry.titleLength = 0;
    if (title && titleLength) {
        entry.title = malloc(titleLength);
        if (!entry.title) abort();
        memcpy(entry.title, title, titleLength);
        entry.titleLength = titleLength;
    }
    pending[pendingCount++] = entry;
}

void silk_menu_commit_pending(void) {
    free_entries(silk_menu_entries, silk_menu_count);
    silk_menu_entries = pending;
    silk_menu_count = pendingCount;
    pending = NULL;
    pendingCount = pendingCapacity = 0;
}

void silk_window_menu_begin_bar(void) { silk_menu_reset_pending(); }

void silk_window_menu_begin(const uint8_t *title, size_t length) {
    SilkMenuEntry entry = {0};
    entry.type = SILK_MENU_BEGIN;
    silk_menu_add(entry, title, length);
}

void silk_window_menu_end(void) {
    SilkMenuEntry entry = {0};
    entry.type = SILK_MENU_END;
    silk_menu_add(entry, NULL, 0);
}

void silk_window_menu_action(const uint8_t *title, size_t length, uint32_t action,
                             uint32_t keyKind, uint32_t keyValue, uint32_t modifiers,
                             uint8_t enabled, uint8_t checked) {
    SilkMenuEntry entry = {0};
    entry.type = SILK_MENU_ACTION;
    entry.action = action;
    entry.keyKind = keyKind;
    entry.keyValue = keyKind == 1 ? silk_ascii_lower(keyValue) : keyValue;
    entry.modifiers = modifiers;
    entry.enabled = enabled;
    entry.checked = checked;
    silk_menu_add(entry, title, length);
}

void silk_window_menu_separator(void) {
    SilkMenuEntry entry = {0};
    entry.type = SILK_MENU_SEPARATOR;
    silk_menu_add(entry, NULL, 0);
}

void silk_window_menu_system(uint32_t item) {
    SilkMenuEntry entry = {0};
    entry.type = SILK_MENU_SYSTEM;
    entry.item = item;
    // The usual shortcuts of system items, so that they work without a visible menu.
    if (item == SILK_ITEM_QUIT) {
        entry.keyKind = 1;
        entry.keyValue = 'q';
        entry.modifiers = SILK_MOD_CONTROL;
    } else if (item == SILK_ITEM_CLOSE_WINDOW) {
        entry.keyKind = 1;
        entry.keyValue = 'w';
        entry.modifiers = SILK_MOD_CONTROL;
    } else if (item == SILK_ITEM_TOGGLE_FULL_SCREEN) {
        entry.keyKind = 2;
        entry.keyValue = 42; // NamedKey.F11
    }
    entry.enabled = 1;
    silk_menu_add(entry, NULL, 0);
}

SilkShortcutMatch silk_menu_match(uint32_t character, uint32_t named, uint32_t modifiers) {
    SilkShortcutMatch match = {0, 0};
    uint32_t mask = SILK_MOD_SHIFT | SILK_MOD_CONTROL | SILK_MOD_ALT | SILK_MOD_META;
    modifiers &= mask;
    character = silk_ascii_lower(character);
    for (size_t i = 0; i < silk_menu_count; i++) {
        SilkMenuEntry *entry = &silk_menu_entries[i];
        if (entry->keyKind == 0) continue;
        if (entry->type != SILK_MENU_ACTION && entry->type != SILK_MENU_SYSTEM) continue;
        if (!entry->enabled) continue;
        if ((entry->modifiers & mask) != modifiers) continue;
        bool hit = entry->keyKind == 1 ? (character && entry->keyValue == character)
                                       : (named && entry->keyValue == named);
        if (!hit) continue;
        match.type = entry->type;
        match.action = entry->type == SILK_MENU_ACTION ? entry->action : entry->item;
        return match;
    }
    return match;
}

// ---------------------------------------------------------------------------------------
// UTF-8 and strings.

size_t silk_utf8_encode(uint32_t c, uint8_t out[4]) {
    if (c < 0x80) {
        out[0] = (uint8_t)c;
        return 1;
    }
    if (c < 0x800) {
        out[0] = (uint8_t)(0xC0 | (c >> 6));
        out[1] = (uint8_t)(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        if (c >= 0xD800 && c < 0xE000) return 0;
        out[0] = (uint8_t)(0xE0 | (c >> 12));
        out[1] = (uint8_t)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (c & 0x3F));
        return 3;
    }
    if (c < 0x110000) {
        out[0] = (uint8_t)(0xF0 | (c >> 18));
        out[1] = (uint8_t)(0x80 | ((c >> 12) & 0x3F));
        out[2] = (uint8_t)(0x80 | ((c >> 6) & 0x3F));
        out[3] = (uint8_t)(0x80 | (c & 0x3F));
        return 4;
    }
    return 0;
}

uint32_t silk_utf8_first(const uint8_t *s, size_t length) {
    if (!s || length == 0) return 0;
    if (s[0] < 0x80) return s[0];
    if ((s[0] & 0xE0) == 0xC0 && length >= 2) return ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    if ((s[0] & 0xF0) == 0xE0 && length >= 3)
        return ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    if ((s[0] & 0xF8) == 0xF0 && length >= 4)
        return ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
               ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    return 0;
}

uint32_t silk_ascii_lower(uint32_t c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

bool silk_utf8_is_control(const uint8_t *text, size_t length) {
    for (size_t i = 0; i < length; i++)
        if (text[i] >= 0x20 && text[i] != 0x7F) return false;
    return true;
}

uint32_t silk_next_dialog_id(void) { return nextDialog++; }

char *silk_strndup(const uint8_t *bytes, size_t length) {
    char *copy = malloc(length + 1);
    if (!copy) abort();
    if (length) memcpy(copy, bytes, length);
    copy[length] = 0;
    return copy;
}

char **silk_split_extensions(const uint8_t *extensions, size_t length, size_t *count) {
    size_t parts = 1;
    for (size_t i = 0; i < length; i++)
        if (extensions[i] == ',') parts++;
    char **list = calloc(parts + 1, sizeof *list);
    if (!list) abort();
    size_t n = 0, start = 0;
    for (size_t i = 0; i <= length; i++) {
        if (i == length || extensions[i] == ',') {
            size_t a = start, b = i;
            while (a < b && (extensions[a] == ' ' || extensions[a] == '.')) a++;
            while (b > a && extensions[b - 1] == ' ') b--;
            if (b > a) list[n++] = silk_strndup(extensions + a, b - a);
            start = i + 1;
        }
    }
    *count = n;
    return list;
}

void silk_free_extensions(char **list) {
    if (!list) return;
    for (char **p = list; *p; p++) free(*p);
    free(list);
}
