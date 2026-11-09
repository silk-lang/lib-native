// File dialogs on Linux run as a separate process, so they never block the event loop:
// kdialog on KDE, zenity elsewhere (whichever exists). The process prints the chosen paths,
// one per line, and exits nonzero when cancelled.
//
// SILK_WINDOW_DIALOG replaces both with a shell command, run as
//   sh -c "$SILK_WINDOW_DIALOG" silk-dialog <open|save> <title> <directory> <name> <extensions>
// for tests and for desktops that prefer another chooser.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "linux.h"

extern char **environ;

typedef struct Dialog {
    uint32_t id;
    pid_t pid;
    int fd;
    char *output;
    size_t length, capacity;
} Dialog;

enum { MAX_DIALOGS = 16 };
static Dialog dialogs[MAX_DIALOGS];
static int dialogCount;

static bool on_path(const char *program) {
    const char *path = getenv("PATH");
    if (!path) return false;
    char candidate[4096];
    while (*path) {
        const char *end = strchr(path, ':');
        size_t length = end ? (size_t)(end - path) : strlen(path);
        if (length && length + strlen(program) + 2 < sizeof candidate) {
            memcpy(candidate, path, length);
            candidate[length] = '/';
            strcpy(candidate + length + 1, program);
            if (access(candidate, X_OK) == 0) return true;
        }
        if (!end) break;
        path = end + 1;
    }
    return false;
}

typedef struct Args {
    char *items[32];
    int count;
} Args;

static void add(Args *args, char *value) {
    if (args->count < 31) args->items[args->count++] = value;
    args->items[args->count] = NULL;
}

static char *format(const char *pattern, const char *a, const char *b) {
    size_t size = strlen(pattern) + strlen(a) + strlen(b) + 1;
    char *out = malloc(size);
    if (!out) abort();
    snprintf(out, size, pattern, a, b);
    return out;
}

static char *join_patterns(char **list, const char *prefix, const char *separator) {
    size_t size = 1;
    for (char **p = list; *p; p++) size += strlen(prefix) + strlen(*p) + strlen(separator);
    char *out = calloc(size, 1);
    if (!out) abort();
    for (char **p = list; *p; p++) {
        if (p != list) strcat(out, separator);
        strcat(out, prefix);
        strcat(out, *p);
    }
    return out;
}

static bool prefer_kdialog(void) {
    const char *desktop = getenv("XDG_CURRENT_DESKTOP");
    return desktop && strstr(desktop, "KDE");
}

uint32_t silk_dialog_open(const uint8_t *titleBytes, size_t titleLength,
                          const uint8_t *directoryBytes, size_t directoryLength,
                          const uint8_t *extensionsBytes, size_t extensionsLength, uint32_t flags,
                          bool save, const uint8_t *nameBytes, size_t nameLength) {
    if (dialogCount == MAX_DIALOGS) return 0;
    char *title = silk_strndup(titleBytes, titleLength);
    char *directory = silk_strndup(directoryBytes, (flags & 8) ? directoryLength : 0);
    char *name = silk_strndup(nameBytes, nameLength);
    char *extensionsText = silk_strndup(extensionsBytes, extensionsLength);
    size_t extensionCount = 0;
    char **extensions = silk_split_extensions(extensionsBytes, extensionsLength, &extensionCount);
    bool directories = (flags & 2) && !(flags & 1);
    bool multiple = (flags & 4) != 0;

    Args args = {{0}, 0};
    char *owned[8] = {0};
    int ownedCount = 0;
    const char *custom = getenv("SILK_WINDOW_DIALOG");
    char *start = directory[0] ? format("%s/%s", directory, name) : strdup(name);
    if (!start) abort();
    owned[ownedCount++] = start;
    bool kdialog = !custom && ((prefer_kdialog() && on_path("kdialog")) || !on_path("zenity"));
    if (!custom && kdialog && !on_path("kdialog")) {
        free(start);
        free(title);
        free(directory);
        free(name);
        free(extensionsText);
        silk_free_extensions(extensions);
        return 0;
    }
    if (custom) {
        add(&args, "/bin/sh");
        add(&args, "-c");
        add(&args, (char *)custom);
        add(&args, "silk-dialog");
        add(&args, save ? "save" : "open");
        add(&args, title);
        add(&args, directory);
        add(&args, name);
        add(&args, extensionsText);
    } else if (kdialog) {
        add(&args, "kdialog");
        add(&args, "--title");
        add(&args, title);
        if (save) {
            add(&args, "--getsavefilename");
            add(&args, start[0] ? start : ".");
        } else if (directories) {
            add(&args, "--getexistingdirectory");
            add(&args, directory[0] ? directory : ".");
        } else {
            add(&args, "--getopenfilename");
            add(&args, directory[0] ? directory : ".");
            if (multiple) {
                add(&args, "--multiple");
                add(&args, "--separate-output");
            }
        }
        if (!directories && extensionCount) {
            char *filter = join_patterns(extensions, "*.", " ");
            owned[ownedCount++] = filter;
            add(&args, filter);
        }
    } else {
        add(&args, "zenity");
        add(&args, "--file-selection");
        char *titleArg = format("--title=%s%s", title, "");
        owned[ownedCount++] = titleArg;
        add(&args, titleArg);
        if (save) add(&args, "--save");
        if (directories) add(&args, "--directory");
        if (multiple && !save) {
            add(&args, "--multiple");
            add(&args, "--separator=\n");
        }
        if (start[0] || directory[0]) {
            char *file = format("--filename=%s%s", save ? start : directory, save ? "" : "/");
            owned[ownedCount++] = file;
            add(&args, file);
        }
        if (!directories && extensionCount) {
            char *patterns = join_patterns(extensions, "*.", " ");
            char *filter = format("--file-filter=%s | %s", extensionsText, patterns);
            free(patterns);
            owned[ownedCount++] = filter;
            add(&args, filter);
        }
    }

    int pipes[2];
    uint32_t id = 0;
    if (pipe2(pipes, O_CLOEXEC) == 0) {
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
        pid_t pid;
        int status = posix_spawnp(&pid, args.items[0], &actions, NULL, args.items, environ);
        posix_spawn_file_actions_destroy(&actions);
        close(pipes[1]);
        if (status == 0) {
            fcntl(pipes[0], F_SETFL, O_NONBLOCK);
            Dialog *dialog = &dialogs[dialogCount++];
            memset(dialog, 0, sizeof *dialog);
            dialog->id = id = silk_next_dialog_id();
            dialog->pid = pid;
            dialog->fd = pipes[0];
        } else {
            close(pipes[0]);
        }
    }
    for (int i = 0; i < ownedCount; i++) free(owned[i]);
    free(title);
    free(directory);
    free(name);
    free(extensionsText);
    silk_free_extensions(extensions);
    return id;
}

int silk_dialog_fds(struct pollfd *fds, int capacity) {
    int count = 0;
    for (int i = 0; i < dialogCount && count < capacity; i++) {
        fds[count].fd = dialogs[i].fd;
        fds[count].events = POLLIN;
        fds[count].revents = 0;
        count++;
    }
    return count;
}

static void finish(Dialog *dialog) {
    int status = 0;
    while (waitpid(dialog->pid, &status, 0) < 0 && errno == EINTR) {
    }
    bool chosen = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    SilkText texts[SILK_MAX_TEXTS];
    size_t count = 0;
    if (chosen && dialog->output) {
        size_t start = 0;
        for (size_t i = 0; i <= dialog->length && count < SILK_MAX_TEXTS; i++) {
            if (i == dialog->length || dialog->output[i] == '\n') {
                if (i > start) {
                    texts[count].bytes = (uint8_t *)dialog->output + start;
                    texts[count].length = i - start;
                    count++;
                }
                start = i + 1;
            }
        }
    }
    SilkWindowEvent event = silk_event(SILK_EVENT_DIALOG_CLOSED, 0);
    event.code = dialog->id;
    event.count = (uint32_t)count;
    silk_queue_push_texts(&event, texts, count);
    free(dialog->output);
    close(dialog->fd);
}

void silk_dialog_dispatch(void) {
    for (int i = 0; i < dialogCount;) {
        Dialog *dialog = &dialogs[i];
        bool done = false;
        for (;;) {
            char buffer[4096];
            ssize_t got = read(dialog->fd, buffer, sizeof buffer);
            if (got > 0) {
                if (dialog->length + (size_t)got > dialog->capacity) {
                    size_t capacity = (dialog->length + (size_t)got) * 2;
                    char *output = realloc(dialog->output, capacity);
                    if (!output) abort();
                    dialog->output = output;
                    dialog->capacity = capacity;
                }
                memcpy(dialog->output + dialog->length, buffer, (size_t)got);
                dialog->length += (size_t)got;
                continue;
            }
            if (got == 0) done = true;
            else if (errno == EINTR) continue;
            break;
        }
        if (done) {
            finish(dialog);
            dialogs[i] = dialogs[--dialogCount];
        } else {
            i++;
        }
    }
}
