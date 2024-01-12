#include "receiver.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <SDL2/SDL_clipboard.h>

#include "device_msg.h"
#include "util/log.h"
#include "util/str.h"

bool
sc_receiver_init(struct sc_receiver *receiver, sc_socket control_socket) {
    bool ok = sc_mutex_init(&receiver->mutex);
    if (!ok) {
        return false;
    }

    receiver->control_socket = control_socket;
    receiver->acksync = NULL;
    receiver->uhid_devices = NULL;

    return true;
}

void
sc_receiver_destroy(struct sc_receiver *receiver) {
    sc_mutex_destroy(&receiver->mutex);
}

static void
process_msg(struct sc_receiver *receiver, struct sc_device_msg *msg) {
    switch (msg->type) {
        case DEVICE_MSG_TYPE_CLIPBOARD: {
            char *current = SDL_GetClipboardText();
            bool same = current && !strcmp(current, msg->clipboard.text);
            SDL_free(current);
            if (same) {
                LOGD("Computer clipboard unchanged");
                return;
            }

            LOGI("Device clipboard copied");
            SDL_SetClipboardText(msg->clipboard.text);
            break;
        }
        case DEVICE_MSG_TYPE_ACK_CLIPBOARD:
            LOGD("Ack device clipboard sequence=%" PRIu64_,
                 msg->ack_clipboard.sequence);

            // This is a programming error to receive this message if there is
            // no ACK synchronization mechanism
            assert(receiver->acksync);

            // Also check at runtime (do not trust the server)
            if (!receiver->acksync) {
                LOGE("Received unexpected ack");
                return;
            }

            sc_acksync_ack(receiver->acksync, msg->ack_clipboard.sequence);
            break;
        case DEVICE_MSG_TYPE_UHID_OUTPUT:
            if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
                char *hex = sc_str_to_hex_string(msg->uhid_output.data,
                                                 msg->uhid_output.size);
                if (hex) {
                    LOGV("UHID output [%" PRIu16 "] %s",
                         msg->uhid_output.id, hex);
                    free(hex);
                } else {
                    LOGV("UHID output [%" PRIu16 "] size=%" PRIu16,
                         msg->uhid_output.id, msg->uhid_output.size);
                }
            }

            // This is a programming error to receive this message if there is
            // no uhid_devices instance
            assert(receiver->uhid_devices);

            // Also check at runtime (do not trust the server)
            if (!receiver->uhid_devices) {
                LOGE("Received unexpected HID output message");
                return;
            }

            struct sc_uhid_receiver *uhid_receiver =
                sc_uhid_devices_get_receiver(receiver->uhid_devices,
                                             msg->uhid_output.id);
            if (uhid_receiver) {
                uhid_receiver->ops->process_output(uhid_receiver,
                                                   msg->uhid_output.data,
                                                   msg->uhid_output.size);
            } else {
                LOGW("No UHID receiver for id %" PRIu16, msg->uhid_output.id);
            }
            break;
    }
}

static ssize_t
process_msgs(struct sc_receiver *receiver, const uint8_t *buf, size_t len) {
    size_t head = 0;
    for (;;) {
        struct sc_device_msg msg;
        ssize_t r = sc_device_msg_deserialize(&buf[head], len - head, &msg);
        if (r == -1) {
            return -1;
        }
        if (r == 0) {
            return head;
        }

        process_msg(receiver, &msg);
        sc_device_msg_destroy(&msg);

        head += r;
        assert(head <= len);
        if (head == len) {
            return head;
        }
    }
}

static int
run_receiver(void *data) {
    struct sc_receiver *receiver = data;

    static uint8_t buf[DEVICE_MSG_MAX_SIZE];
    size_t head = 0;

    for (;;) {
        assert(head < DEVICE_MSG_MAX_SIZE);
        ssize_t r = net_recv(receiver->control_socket, buf + head,
                             DEVICE_MSG_MAX_SIZE - head);
        if (r <= 0) {
            LOGD("Receiver stopped");
            break;
        }

        head += r;
        ssize_t consumed = process_msgs(receiver, buf, head);
        if (consumed == -1) {
            // an error occurred
            break;
        }

        if (consumed) {
            head -= consumed;
            // shift the remaining data in the buffer
            memmove(buf, &buf[consumed], head);
        }
    }

    return 0;
}

bool
sc_receiver_start(struct sc_receiver *receiver) {
    LOGD("Starting receiver thread");

    bool ok = sc_thread_create(&receiver->thread, run_receiver,
                               "scrcpy-receiver", receiver);
    if (!ok) {
        LOGE("Could not start receiver thread");
        return false;
    }

    return true;
}

void
sc_receiver_join(struct sc_receiver *receiver) {
    sc_thread_join(&receiver->thread, NULL);
}
