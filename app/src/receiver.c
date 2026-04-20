#include "receiver.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL_clipboard.h>

#include "device_msg.h"
#include "events.h"
#include "util/log.h"
#include "util/str.h"
#include "util/thread.h"

struct sc_uhid_output_task_data {
    struct sc_uhid_devices *uhid_devices;
    uint16_t id;
    uint16_t size;
    uint8_t *data;
};

struct sc_image_clipboard_data {
    uint8_t *data;
    uint32_t size;
    char *mimetype;
};

static const void * SDLCALL
clipboard_data_callback(void *userdata, const char *mime_type, size_t *size) {
    struct sc_image_clipboard_data *image_data = userdata;
    (void) mime_type;
    *size = image_data->size;
    return image_data->data;
}

static void SDLCALL
clipboard_cleanup_callback(void *userdata) {
    struct sc_image_clipboard_data *image_data = userdata;
    free(image_data->data);
    free(image_data->mimetype);
    free(image_data);
}

bool
sc_receiver_init(struct sc_receiver *receiver, sc_socket control_socket,
                 const struct sc_receiver_callbacks *cbs, void *cbs_userdata) {
    bool ok = sc_mutex_init(&receiver->mutex);
    if (!ok) {
        return false;
    }

    receiver->control_socket = control_socket;
    receiver->acksync = NULL;
    receiver->uhid_devices = NULL;

    assert(cbs && cbs->on_ended);
    receiver->cbs = cbs;
    receiver->cbs_userdata = cbs_userdata;

    return true;
}

void
sc_receiver_destroy(struct sc_receiver *receiver) {
    sc_mutex_destroy(&receiver->mutex);
}

static void
task_set_clipboard(void *userdata) {
    assert(sc_thread_get_id() == SC_MAIN_THREAD_ID);

    char *text = userdata;

    char *current = SDL_GetClipboardText();
    bool same = current && !strcmp(current, text);
    SDL_free(current);
    if (same) {
        LOGD("Computer clipboard unchanged");
    } else {
        bool ok = SDL_SetClipboardText(text);
        if (ok) {
            LOGI("Device clipboard copied");
        } else {
            LOGE("Could not set clipboard: %s", SDL_GetError());
        }
    }

    free(text);
}

static void
task_set_image_clipboard(void *userdata) {
    assert(sc_thread_get_id() == SC_MAIN_THREAD_ID);

    struct sc_device_msg *msg = userdata;

    struct sc_image_clipboard_data *image_data =
        malloc(sizeof(struct sc_image_clipboard_data));
    if (!image_data) {
        LOG_OOM();
        free(msg->image_clipboard.data);
        free(msg->image_clipboard.mimetype);
        free(msg);
        return;
    }

    image_data->data = msg->image_clipboard.data;
    image_data->size = msg->image_clipboard.size;
    image_data->mimetype = msg->image_clipboard.mimetype;

    const char *mime_types[1] = { image_data->mimetype };

    bool ok = SDL_SetClipboardData(clipboard_data_callback,
                                    clipboard_cleanup_callback,
                                    image_data,
                                    mime_types, 1);
    if (ok) {
        LOGI("Device image clipboard copied");
    } else {
        LOGE("Could not set image clipboard: %s", SDL_GetError());
        free(image_data->data);
        free(image_data->mimetype);
        free(image_data);
    }

    free(msg);
}

static void
task_uhid_output(void *userdata) {
    assert(sc_thread_get_id() == SC_MAIN_THREAD_ID);

    struct sc_uhid_output_task_data *data = userdata;

    sc_uhid_devices_process_hid_output(data->uhid_devices, data->id, data->data,
                                       data->size);

    free(data->data);
    free(data);
}

static void
process_msg(struct sc_receiver *receiver, struct sc_device_msg *msg) {
    switch (msg->type) {
        case DEVICE_MSG_TYPE_CLIPBOARD: {
            // Take ownership of the text (do not destroy the msg)
            char *text = msg->clipboard.text;

            bool ok = sc_post_to_main_thread(task_set_clipboard, text);
            if (!ok) {
                LOGW("Could not post clipboard to main thread");
                free(text);
                return;
            }

            break;
        }
        case DEVICE_MSG_TYPE_IMAGE_CLIPBOARD: {
            // Create a copy of the message to send to the main thread
            struct sc_device_msg *msg_copy = malloc(sizeof(struct sc_device_msg));
            if (!msg_copy) {
                LOG_OOM();
                return;
            }

            msg_copy->type = DEVICE_MSG_TYPE_IMAGE_CLIPBOARD;
            msg_copy->image_clipboard.data = malloc(msg->image_clipboard.size);
            if (!msg_copy->image_clipboard.data) {
                LOG_OOM();
                free(msg_copy);
                return;
            }
            memcpy(msg_copy->image_clipboard.data, msg->image_clipboard.data, msg->image_clipboard.size);
            msg_copy->image_clipboard.size = msg->image_clipboard.size;

            // Duplicate the mimetype string
            size_t mimetype_len = strlen(msg->image_clipboard.mimetype);
            msg_copy->image_clipboard.mimetype = malloc(mimetype_len + 1);
            if (!msg_copy->image_clipboard.mimetype) {
                LOG_OOM();
                free(msg_copy->image_clipboard.data);
                free(msg_copy);
                return;
            }
            strcpy(msg_copy->image_clipboard.mimetype, msg->image_clipboard.mimetype);

            bool ok = sc_post_to_main_thread(task_set_image_clipboard, msg_copy);
            if (!ok) {
                LOGW("Could not post image clipboard to main thread");
                free(msg_copy->image_clipboard.data);
                free(msg_copy->image_clipboard.mimetype);
                free(msg_copy);
                return;
            }

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
            // No allocation to free in the msg
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

            if (!receiver->uhid_devices) {
                LOGE("Received unexpected HID output message");
                sc_device_msg_destroy(msg);
                return;
            }

            struct sc_uhid_output_task_data *data = malloc(sizeof(*data));
            if (!data) {
                LOG_OOM();
                return;
            }

            // It is guaranteed that these pointers will still be valid when
            // the main thread will process them (the main thread will stop
            // processing SC_EVENT_RUN_ON_MAIN_THREAD on exit, when everything
            // gets deinitialized)
            data->uhid_devices = receiver->uhid_devices;
            data->id = msg->uhid_output.id;
            data->data = msg->uhid_output.data; // take ownership
            data->size = msg->uhid_output.size;

            bool ok = sc_post_to_main_thread(task_uhid_output, data);
            if (!ok) {
                LOGW("Could not post UHID output to main thread");
                free(data->data);
                free(data);
                return;
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
        // the device msg must be destroyed by process_msg()

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

    bool error = false;

    for (;;) {
        assert(head < DEVICE_MSG_MAX_SIZE);
        ssize_t r = net_recv(receiver->control_socket, buf + head,
                             DEVICE_MSG_MAX_SIZE - head);
        if (r <= 0) {
            LOGD("Receiver stopped");
            // device disconnected: keep error=false
            break;
        }

        head += r;
        ssize_t consumed = process_msgs(receiver, buf, head);
        if (consumed == -1) {
            // an error occurred
            error = true;
            break;
        }

        if (consumed) {
            head -= consumed;
            // shift the remaining data in the buffer
            memmove(buf, &buf[consumed], head);
        }
    }

    receiver->cbs->on_ended(receiver, error, receiver->cbs_userdata);

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
