#ifndef DEVICEEVENT_H
#define DEVICEEVENT_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#define DEVICE_EVENT_QUEUE_SIZE 64
#define DEVICE_EVENT_TEXT_MAX_LENGTH 4093
#define DEVICE_EVENT_SERIALIZED_MAX_SIZE (3 + DEVICE_EVENT_TEXT_MAX_LENGTH)

enum device_event_type {
    DEVICE_EVENT_TYPE_GET_CLIPBOARD,
};

struct device_event {
    enum device_event_type type;
    union {
        struct {
            char *text; // owned, to be freed by SDL_free()
        } clipboard_event;
    };
};

// return the number of bytes consumed (0 for no event available, -1 on error)
ssize_t
device_event_deserialize(const unsigned char *buf, size_t len,
                         struct device_event *event);

void
device_event_destroy(struct device_event *event);

#endif
