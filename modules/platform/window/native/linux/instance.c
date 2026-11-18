// Single instance: the first launch listens on an abstract Unix socket named for the app and
// the user; a later one connects, writes its arguments and ends.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "../bridge.h"
#include "linux.h"

static int listener = -1;

static socklen_t instance_address(struct sockaddr_un *address, const uint8_t *name,
                                  size_t length) {
    memset(address, 0, sizeof *address);
    address->sun_family = AF_UNIX;
    // A leading zero byte puts the name in the abstract namespace: no file to clean up.
    int prefix = snprintf(address->sun_path + 1, sizeof address->sun_path - 1, "silk-instance.%u.",
                          (unsigned)getuid());
    size_t room = sizeof address->sun_path - 1 - (size_t)prefix;
    if (length > room) length = room;
    memcpy(address->sun_path + 1 + prefix, name, length);
    return (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + (size_t)prefix + length);
}

static bool hand_over(const struct sockaddr_un *address, socklen_t size, const uint8_t *arguments,
                      size_t length) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;
    if (connect(fd, (const struct sockaddr *)address, size) != 0) {
        close(fd);
        return false;
    }
    size_t sent = 0;
    while (sent < length) {
        ssize_t wrote = write(fd, arguments + sent, length - sent);
        if (wrote < 0 && errno == EINTR) continue;
        if (wrote <= 0) break;
        sent += (size_t)wrote;
    }
    close(fd);
    return true;
}

uint8_t silk_window_claim_instance(const uint8_t *name, size_t nameLength,
                                   const uint8_t *arguments, size_t argumentsLength) {
    struct sockaddr_un address;
    socklen_t size = instance_address(&address, name, nameLength);
    if (listener >= 0) return hand_over(&address, size, arguments, argumentsLength) ? 0 : 1;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) return 1;
    if (bind(fd, (const struct sockaddr *)&address, size) == 0 && listen(fd, 8) == 0) {
        listener = fd;
        return 1;
    }
    int failure = errno;
    close(fd);
    if (failure != EADDRINUSE) return 1;
    return hand_over(&address, size, arguments, argumentsLength) ? 0 : 1;
}

int silk_instance_fd(void) { return listener; }

static void read_launch(int fd) {
    // The sender writes everything and closes, so read to the end, waiting at most a second.
    struct timeval limit = {1, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof limit);
    size_t capacity = 4096, length = 0;
    uint8_t *bytes = malloc(capacity);
    if (!bytes) return;
    for (;;) {
        if (length == capacity) {
            if (capacity >= (1u << 20)) break;
            uint8_t *grown = realloc(bytes, capacity * 2);
            if (!grown) break;
            bytes = grown;
            capacity *= 2;
        }
        ssize_t got = read(fd, bytes + length, capacity - length);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        length += (size_t)got;
    }
    SilkText texts[SILK_MAX_TEXTS];
    size_t count = 0, start = 0;
    for (size_t i = 0; i < length && count < SILK_MAX_TEXTS; i++) {
        if (bytes[i] != 0) continue;
        texts[count].bytes = bytes + start;
        texts[count].length = i - start;
        count++;
        start = i + 1;
    }
    SilkWindowEvent event = silk_event(SILK_EVENT_OPEN_REQUESTED, 0);
    event.count = (uint32_t)count;
    silk_queue_push_texts(&event, texts, count);
    free(bytes);
}

void silk_instance_dispatch(void) {
    if (listener < 0) return;
    for (;;) {
        int fd = accept4(listener, NULL, NULL, SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR) continue;
            return;
        }
        read_launch(fd);
        close(fd);
    }
}
