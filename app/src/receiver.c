#include "receiver.h"

#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_clipboard.h>

#include "config.h"
#include "device_event.h"
#include "events.h"
#include "lock_util.h"
#include "log.h"

bool
receiver_init(struct receiver *receiver, socket_t control_socket) {
    if (!(receiver->mutex = SDL_CreateMutex())) {
        return false;
    }
    receiver->control_socket = control_socket;
    return true;
}

void
receiver_destroy(struct receiver *receiver) {
    SDL_DestroyMutex(receiver->mutex);
}

static void
process_event(struct receiver *receiver, struct device_event *event) {
    switch (event->type) {
        case DEVICE_EVENT_TYPE_GET_CLIPBOARD:
            SDL_SetClipboardText(event->clipboard_event.text);
            break;
    }
}

static ssize_t
process_events(struct receiver *receiver, const unsigned char *buf,
               size_t len) {
    size_t head = 0;
    for (;;) {
        struct device_event event;
        ssize_t r = device_event_deserialize(&buf[head], len - head, &event);
        if (r == -1) {
            return -1;
        }
        if (r == 0) {
            return head;
        }

        process_event(receiver, &event);
        device_event_destroy(&event);

        head += r;
        SDL_assert(head <= len);
        if (head == len) {
            return head;
        }
    }
}

static int
run_receiver(void *data) {
    struct receiver *receiver = data;

    unsigned char buf[DEVICE_EVENT_SERIALIZED_MAX_SIZE];
    size_t head = 0;

    for (;;) {
        SDL_assert(head < DEVICE_EVENT_SERIALIZED_MAX_SIZE);
        ssize_t r = net_recv(receiver->control_socket, buf,
                             DEVICE_EVENT_SERIALIZED_MAX_SIZE - head);
        if (r <= 0) {
            LOGD("Receiver stopped");
            break;
        }

        ssize_t consumed = process_events(receiver, buf, r);
        if (consumed == -1) {
            // an error occurred
            break;
        }

        if (consumed) {
            // shift the remaining data in the buffer
            memmove(buf, &buf[consumed], r - consumed);
            head = r - consumed;
        }
    }

    return 0;
}

bool
receiver_start(struct receiver *receiver) {
    LOGD("Starting receiver thread");

    receiver->thread = SDL_CreateThread(run_receiver, "receiver", receiver);
    if (!receiver->thread) {
        LOGC("Could not start receiver thread");
        return false;
    }

    return true;
}

void
receiver_join(struct receiver *receiver) {
    SDL_WaitThread(receiver->thread, NULL);
}
