#include "device_msg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util/binary.h"
#include "util/log.h"

// Read a string (2-byte length + UTF-8 content) at *pos, and update *pos.
// An empty string is read as NULL.
// Return 1 on success, 0 if the string is not complete, -1 on error.
static int
read_media_string(const uint8_t *buf, size_t len, size_t *pos, char **out) {
    if (len - *pos < 2) {
        return 0; // not available
    }
    size_t str_len = sc_read16be(&buf[*pos]);
    *pos += 2;
    if (str_len > len - *pos) {
        return 0; // not available
    }

    char *str = NULL;
    if (str_len) {
        str = malloc(str_len + 1);
        if (!str) {
            LOG_OOM();
            return -1;
        }
        memcpy(str, &buf[*pos], str_len);
        str[str_len] = '\0';
    }
    *pos += str_len;
    *out = str;
    return 1;
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
        case DEVICE_MSG_TYPE_MEDIA_UPDATE: {
            if (len < 22) {
                // type (1) + state (1) + position (8) + capabilities (4)
                // + duration (8)
                return 0; // not available
            }
            struct sc_media_update *update = &msg->media_update;
            update->state = buf[1];
            update->position_ms = (int64_t) sc_read64be(&buf[2]);
            update->capabilities = sc_read32be(&buf[10]);
            update->duration_ms = (int64_t) sc_read64be(&buf[14]);
            update->title = NULL;
            update->artist = NULL;
            update->album = NULL;

            size_t pos = 22;
            char **strings[] = {&update->title, &update->artist,
                                &update->album};
            for (size_t i = 0; i < ARRAY_LEN(strings); ++i) {
                int r = read_media_string(buf, len, &pos, strings[i]);
                if (r <= 0) {
                    sc_device_msg_destroy(msg);
                    return r;
                }
            }

            return pos;
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
            free(msg->media_update.title);
            free(msg->media_update.artist);
            free(msg->media_update.album);
            break;
        default:
            // nothing to do
            break;
    }
}
