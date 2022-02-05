#include "receiver.h"

#include <assert.h>
#include <SDL2/SDL_clipboard.h>

#include "device_msg.h"
#include "util/log.h"

bool
receiver_init(struct receiver *receiver, sc_socket control_socket,
              struct sc_acksync *acksync) {
    bool ok = sc_mutex_init(&receiver->mutex);
    if (!ok) {
        return false;
    }

    receiver->control_socket = control_socket;
    receiver->acksync = acksync;

    return true;
}

void
receiver_destroy(struct receiver *receiver) {
    sc_mutex_destroy(&receiver->mutex);
}

static void
process_msg(struct receiver *receiver, struct device_msg *msg) {
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
            assert(receiver->acksync);
            LOGD("Ack device clipboard sequence=%" PRIu64_,
                 msg->ack_clipboard.sequence);
            sc_acksync_ack(receiver->acksync, msg->ack_clipboard.sequence);
            break;
    }
}

static ssize_t
process_msgs(struct receiver *receiver, const unsigned char *buf, size_t len) {
    size_t head = 0;
    for (;;) {
        struct device_msg msg;
        ssize_t r = device_msg_deserialize(&buf[head], len - head, &msg);
        if (r == -1) {
            return -1;
        }
        if (r == 0) {
            return head;
        }

        process_msg(receiver, &msg);
        device_msg_destroy(&msg);

        head += r;
        assert(head <= len);
        if (head == len) {
            return head;
        }
    }
}

static int
run_receiver(void *data) {
    struct receiver *receiver = data;

    static unsigned char buf[DEVICE_MSG_MAX_SIZE];
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
receiver_start(struct receiver *receiver) {
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
receiver_join(struct receiver *receiver) {
    sc_thread_join(&receiver->thread, NULL);
}
