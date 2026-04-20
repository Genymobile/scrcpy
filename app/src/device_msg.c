#include "device_msg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util/binary.h"
#include "util/log.h"

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
            char *text = malloc(clipboard_len + 1);
            if (!text) {
                LOG_OOM();
                return -1;
            }
            if (clipboard_len) {
                memcpy(text, &buf[5], clipboard_len);
            }
            text[clipboard_len] = '\0';

            msg->clipboard.text = text;
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
            if (size > len - 5) {
                return 0; // not available
            }
            uint8_t *data = malloc(size);
            if (!data) {
                LOG_OOM();
                return -1;
            }
            if (size) {
                memcpy(data, &buf[5], size);
            }

            msg->uhid_output.id = id;
            msg->uhid_output.size = size;
            msg->uhid_output.data = data;

            return 5 + size;
        }
        case DEVICE_MSG_TYPE_IMAGE_CLIPBOARD: {
            if (len < 9) {
                // at least type + mimetype length (4 bytes) + data length (4 bytes)
                return 0; // no complete message
            }

            uint32_t mimetype_len = sc_read32be(&buf[1]);
            uint32_t data_len = sc_read32be(&buf[5]);

            if (mimetype_len + data_len > len - 9) {
                return 0; // no complete message
            }

            char *mimetype = malloc(mimetype_len + 1);
            if (!mimetype) {
                LOG_OOM();
                return -1;
            }
            if (mimetype_len) {
                memcpy(mimetype, &buf[9], mimetype_len);
            }
            mimetype[mimetype_len] = '\0';

            uint8_t *image_data = malloc(data_len);
            if (!image_data) {
                LOG_OOM();
                free(mimetype);
                return -1;
            }
            if (data_len) {
                memcpy(image_data, &buf[9 + mimetype_len], data_len);
            }

            msg->image_clipboard.mimetype = mimetype;
            msg->image_clipboard.data = image_data;
            msg->image_clipboard.size = data_len;

            return 9 + mimetype_len + data_len;
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
        case DEVICE_MSG_TYPE_IMAGE_CLIPBOARD:
            free(msg->image_clipboard.data);
            free(msg->image_clipboard.mimetype);
            break;
        default:
            // nothing to do
            break;
    }
}
