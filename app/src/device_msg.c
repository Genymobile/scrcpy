#include "device_msg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util/binary.h"
#include "util/log.h"

static int read_message(uint8_t **target, const uint8_t *src, const uint16_t size) {
    uint8_t *data = malloc(size + 1);
    if (!data) {
        LOG_OOM();
        return -1;
    }
    if (size) {
        data[size] = '\0';
        memcpy(data, src, size);
    }
    *target = data;
    return 0;
}

ssize_t
sc_device_msg_deserialize(const uint8_t *buf, size_t len,
                          struct sc_device_msg *msg) {
    if (!len) {
        return 0; // no message
    }

    msg->type = buf[0];
    switch (msg->type) {
        case DEVICE_MSG_TYPE_CLIPBOARD: {
            if (len < 5) {
                // at least type + empty string length
                return 0; // no complete message
            }
            size_t clipboard_len = sc_read32be(&buf[1]);
            if (clipboard_len > len - 5) {
                return 0; // no complete message
            }
            if (read_message((uint8_t **)&msg->clipboard.text, &buf[5], clipboard_len) == -1) {
                return -1;
            }

            return 5 + clipboard_len;
        }
        case DEVICE_MSG_TYPE_ACK_CLIPBOARD: {
            if (len < 9) {
                return 0; // no complete message
            }
            uint64_t sequence = sc_read64be(&buf[1]);
            msg->ack_clipboard.sequence = sequence;
            return 9;
        }
        case DEVICE_MSG_TYPE_UHID_OUTPUT: {
            if (len < 5) {
                // at least id + size
                return 0; // not available
            }
            uint16_t id = sc_read16be(&buf[1]);
            size_t size = sc_read16be(&buf[3]);
            if (size < len - 5) {
                return 0; // not available
            }

            msg->uhid_output.id = id;
            msg->uhid_output.size = size;
            if (read_message(&msg->uhid_output.data, &buf[5], size) == -1) {
                return -1;
            }

            return 5 + size;
        case DEVICE_MSG_TYPE_MEDIA_UPDATE: {
            if (len < 5) {
                // at least id + size
                return 0; // not available
            }
            uint16_t id = sc_read16be(&buf[1]);
            size_t size = sc_read16be(&buf[3]);
            if (size < len - 5) {
                return 0; // not available
            }

            msg->media_update.id = id;
            msg->media_update.size = size;
            if (read_message(&msg->media_update.data, &buf[5], size) == -1) {
                return -1;
            }

            return 5 + size;
        }
        case DEVICE_MSG_TYPE_MEDIA_REMOVE: {
            if (len < 3) {
                // at least id
                return 0; // not available
            }
            uint16_t id = sc_read16be(&buf[1]);
            msg->media_remove.id = id;
            return 3;
            }
        }
        default:
            LOGW("Unknown device message type: %d", (int) msg->type);
            return -1; // error, we cannot recover
    }
}

void
sc_device_msg_destroy(struct sc_device_msg *msg) {
    switch (msg->type) {
        case DEVICE_MSG_TYPE_CLIPBOARD:
            free(msg->clipboard.text);
            break;
        case DEVICE_MSG_TYPE_UHID_OUTPUT:
            free(msg->uhid_output.data);
            break;
        case DEVICE_MSG_TYPE_MEDIA_UPDATE:
            free(msg->media_update.data);
            break;
        default:
            // nothing to do
            break;
    }
}
