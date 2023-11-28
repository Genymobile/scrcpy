#include "device_msg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util/binary.h"
#include "util/log.h"

ssize_t
device_msg_deserialize(const unsigned char *buf, size_t len,
                       struct device_msg *msg) {
    if (len < 5) {
        // at least type + empty string length
        return 0; // not available
    }

    msg->type = buf[0];
    switch (msg->type) {
        case DEVICE_MSG_TYPE_CLIPBOARD: {
            size_t clipboard_len = sc_read32be(&buf[1]);
            if (clipboard_len > len - 5) {
                return 0; // not available
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
            uint64_t sequence = sc_read64be(&buf[1]);
            msg->ack_clipboard.sequence = sequence;
            return 9;
        }
        case DEVICE_MSG_TYPE_UHID_DATA: {
            uint32_t id = sc_read32be(&buf[1]);
            size_t uhid_data_len = sc_read32be(&buf[5]);
            if (uhid_data_len > len - 9) {
                return 0; // not available
            }
            uint8_t *uhid_data = malloc(uhid_data_len);
            if (!uhid_data) {
                LOG_OOM();
                return -1;
            }
            if (uhid_data_len) {
                memcpy(uhid_data, &buf[9], uhid_data_len);
            }
            msg->uhid_data.id = id;
            msg->uhid_data.data = uhid_data;
            msg->uhid_data.len = uhid_data_len;
            return 9 + uhid_data_len;
        }
        default:
            LOGW("Unknown device message type: %d", (int) msg->type);
            return -1; // error, we cannot recover
    }
}

void
device_msg_destroy(struct device_msg *msg) {
    switch (msg->type) {
        case DEVICE_MSG_TYPE_CLIPBOARD:
            free(msg->clipboard.text);
            break;
        case DEVICE_MSG_TYPE_UHID_DATA:
            free(msg->uhid_data.data);
            break;
        default:
            // nothing to do
            break;
    }
}
