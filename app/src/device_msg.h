#ifndef SC_DEVICEMSG_H
#define SC_DEVICEMSG_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define DEVICE_MSG_MAX_SIZE (1 << 18) // 256k
// type: 1 byte; length: 4 bytes
#define DEVICE_MSG_TEXT_MAX_LENGTH (DEVICE_MSG_MAX_SIZE - 5)

enum sc_device_msg_type {
    DEVICE_MSG_TYPE_CLIPBOARD,
    DEVICE_MSG_TYPE_ACK_CLIPBOARD,
    DEVICE_MSG_TYPE_UHID_OUTPUT,
    DEVICE_MSG_TYPE_MEDIA_UPDATE,
};

// Values must be kept in sync with the server (MediaManager.java)
enum sc_media_state {
    SC_MEDIA_STATE_STOPPED,
    SC_MEDIA_STATE_PLAYING,
    SC_MEDIA_STATE_PAUSED,
};

#define SC_MEDIA_CAP_PLAY (1 << 0)
#define SC_MEDIA_CAP_PAUSE (1 << 1)
#define SC_MEDIA_CAP_NEXT (1 << 2)
#define SC_MEDIA_CAP_PREVIOUS (1 << 3)
#define SC_MEDIA_CAP_SEEK (1 << 4)

#define SC_MEDIA_POSITION_UNKNOWN INT64_C(-1)
#define SC_MEDIA_DURATION_UNKNOWN INT64_C(-1)

// State and metadata of a media session of the device
struct sc_media_update {
    enum sc_media_state state;
    int64_t position_ms; // SC_MEDIA_POSITION_UNKNOWN if unknown
    uint32_t capabilities; // SC_MEDIA_CAP_* bitmask
    int64_t duration_ms; // SC_MEDIA_DURATION_UNKNOWN if unknown
    char *title; // owned, to be freed by free(), may be NULL
    char *artist; // owned, to be freed by free(), may be NULL
    char *album; // owned, to be freed by free(), may be NULL
};

struct sc_device_msg {
    enum sc_device_msg_type type;
    union {
        struct {
            char *text; // owned, to be freed by free()
        } clipboard;
        struct {
            uint64_t sequence;
        } ack_clipboard;
        struct {
            uint16_t id;
            uint16_t size;
            uint8_t *data; // owned, to be freed by free()
        } uhid_output;
        struct sc_media_update media_update;
    };
};

// return the number of bytes consumed (0 for no msg available, -1 on error)
ssize_t
sc_device_msg_deserialize(const uint8_t *buf, size_t len,
                          struct sc_device_msg *msg);

void
sc_device_msg_destroy(struct sc_device_msg *msg);

#endif
