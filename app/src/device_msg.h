#ifndef DEVICEMSG_H
#define DEVICEMSG_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "config.h"

#define DEVICE_MSG_MAX_SIZE (1 << 18) // 256k
// type: 1 byte; length: 4 bytes
#define DEVICE_MSG_TEXT_MAX_LENGTH (DEVICE_MSG_MAX_SIZE - 5)

enum device_msg_type {
    DEVICE_MSG_TYPE_CLIPBOARD,
};

struct device_msg {
    enum device_msg_type type;
    union {
        struct {
            char *text; // owned, to be freed by SDL_free()
        } clipboard;
    };
};

// return the number of bytes consumed (0 for no msg available, -1 on error)
ssize_t
device_msg_deserialize(const unsigned char *buf, size_t len,
                       struct device_msg *msg);

void
device_msg_destroy(struct device_msg *msg);

#endif
