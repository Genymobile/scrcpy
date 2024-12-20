#include "aoa_hid.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <libusb-1.0/libusb.h>

#include "events.h"
#include "util/log.h"
#include "util/str.h"
#include "util/tick.h"
#include "util/vector.h"

// See <https://source.android.com/devices/accessories/aoa2#hid-support>.
#define ACCESSORY_REGISTER_HID 54
#define ACCESSORY_SET_HID_REPORT_DESC 56
#define ACCESSORY_SEND_HID_EVENT 57
#define ACCESSORY_UNREGISTER_HID 55

#define DEFAULT_TIMEOUT 1000

// Drop droppable events above this limit
#define SC_AOA_EVENT_QUEUE_LIMIT 60

struct sc_vec_hid_ids SC_VECTOR(uint16_t);

static void
sc_hid_input_log(const struct sc_hid_input *hid_input) {
    // HID input: [00] FF FF FF FF...
    assert(hid_input->size);
    char *hex = sc_str_to_hex_string(hid_input->data, hid_input->size);
    if (!hex) {
        return;
    }
    LOGV("HID input: [%" PRIu16 "] %s", hid_input->hid_id, hex);
    free(hex);
}

static void
sc_hid_open_log(const struct sc_hid_open *hid_open) {
    // HID open: [00] FF FF FF FF...
    assert(hid_open->report_desc_size);
    char *hex = sc_str_to_hex_string(hid_open->report_desc,
                                     hid_open->report_desc_size);
    if (!hex) {
        return;
    }
    LOGV("HID open: [%" PRIu16 "] %s", hid_open->hid_id, hex);
    free(hex);
}

static void
sc_hid_close_log(const struct sc_hid_close *hid_close) {
    // HID close: [00]
    LOGV("HID close: [%" PRIu16 "]", hid_close->hid_id);
}

bool
sc_aoa_init(struct sc_aoa *aoa, struct sc_usb *usb,
            struct sc_acksync *acksync) {
    sc_vecdeque_init(&aoa->queue);

    // Add 4 to support 4 non-droppable events without re-allocation
    if (!sc_vecdeque_reserve(&aoa->queue, SC_AOA_EVENT_QUEUE_LIMIT + 4)) {
        return false;
    }

    if (!sc_mutex_init(&aoa->mutex)) {
        sc_vecdeque_destroy(&aoa->queue);
        return false;
    }

    if (!sc_cond_init(&aoa->event_cond)) {
        sc_mutex_destroy(&aoa->mutex);
        sc_vecdeque_destroy(&aoa->queue);
        return false;
    }

    aoa->stopped = false;
    aoa->acksync = acksync;
    aoa->usb = usb;

    return true;
}

void
sc_aoa_destroy(struct sc_aoa *aoa) {
    sc_vecdeque_destroy(&aoa->queue);

    sc_cond_destroy(&aoa->event_cond);
    sc_mutex_destroy(&aoa->mutex);
}

static bool
sc_aoa_register_hid(struct sc_aoa *aoa, uint16_t accessory_id,
                    uint16_t report_desc_size) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_REGISTER_HID;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): total length of the HID report descriptor
    uint16_t value = accessory_id;
    uint16_t index = report_desc_size;
    unsigned char *data = NULL;
    uint16_t length = 0;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, data, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        LOGE("REGISTER_HID: libusb error: %s", libusb_strerror(result));
        sc_usb_check_disconnected(aoa->usb, result);
        return false;
    }

    return true;
}

static bool
sc_aoa_set_hid_report_desc(struct sc_aoa *aoa, uint16_t accessory_id,
                           const uint8_t *report_desc,
                           uint16_t report_desc_size) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_SET_HID_REPORT_DESC;
    /**
     * If the HID descriptor is longer than the endpoint zero max packet size,
     * the descriptor will be sent in multiple ACCESSORY_SET_HID_REPORT_DESC
     * commands. The data for the descriptor must be sent sequentially
     * if multiple packets are needed.
     * <https://source.android.com/devices/accessories/aoa2.html#hid-support>
     *
     * libusb handles packet abstraction internally, so we don't need to care
     * about bMaxPacketSize0 here.
     *
     * See <https://libusb.sourceforge.io/api-1.0/libusb_packetoverflow.html>
     */
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): offset of data in descriptor
    uint16_t value = accessory_id;
    uint16_t index = 0;
    // libusb_control_transfer expects a pointer to non-const
    unsigned char *data = (unsigned char *) report_desc;
    uint16_t length = report_desc_size;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, data, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        LOGE("SET_HID_REPORT_DESC: libusb error: %s", libusb_strerror(result));
        sc_usb_check_disconnected(aoa->usb, result);
        return false;
    }

    return true;
}

static bool
sc_aoa_send_hid_event(struct sc_aoa *aoa,
                      const struct sc_hid_input *hid_input) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_SEND_HID_EVENT;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): 0 (unused)
    uint16_t value = hid_input->hid_id;
    uint16_t index = 0;
    unsigned char *data = (uint8_t *) hid_input->data; // discard const
    uint16_t length = hid_input->size;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, data, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        LOGE("SEND_HID_EVENT: libusb error: %s", libusb_strerror(result));
        sc_usb_check_disconnected(aoa->usb, result);
        return false;
    }

    return true;
}

static bool
sc_aoa_unregister_hid(struct sc_aoa *aoa, uint16_t accessory_id) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_UNREGISTER_HID;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): 0
    uint16_t value = accessory_id;
    uint16_t index = 0;
    unsigned char *data = NULL;
    uint16_t length = 0;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, data, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        LOGE("UNREGISTER_HID: libusb error: %s", libusb_strerror(result));
        sc_usb_check_disconnected(aoa->usb, result);
        return false;
    }

    return true;
}

static bool
sc_aoa_setup_hid(struct sc_aoa *aoa, uint16_t accessory_id,
                 const uint8_t *report_desc, uint16_t report_desc_size) {
    bool ok = sc_aoa_register_hid(aoa, accessory_id, report_desc_size);
    if (!ok) {
        return false;
    }

    ok = sc_aoa_set_hid_report_desc(aoa, accessory_id, report_desc,
                                    report_desc_size);
    if (!ok) {
        if (!sc_aoa_unregister_hid(aoa, accessory_id)) {
            LOGW("Could not unregister HID");
        }
        return false;
    }

    return true;
}

bool
sc_aoa_push_input_with_ack_to_wait(struct sc_aoa *aoa,
                                   const struct sc_hid_input *hid_input,
                                   uint64_t ack_to_wait) {
    if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
        sc_hid_input_log(hid_input);
    }

    sc_mutex_lock(&aoa->mutex);

    bool pushed = false;

    size_t size = sc_vecdeque_size(&aoa->queue);
    if (size < SC_AOA_EVENT_QUEUE_LIMIT) {
        bool was_empty = sc_vecdeque_is_empty(&aoa->queue);

        struct sc_aoa_event *aoa_event =
            sc_vecdeque_push_hole_noresize(&aoa->queue);
        aoa_event->type = SC_AOA_EVENT_TYPE_INPUT;
        aoa_event->input.hid = *hid_input;
        aoa_event->input.ack_to_wait = ack_to_wait;
        pushed = true;

        if (was_empty) {
            sc_cond_signal(&aoa->event_cond);
        }
    }
    // Otherwise, the event is discarded

    sc_mutex_unlock(&aoa->mutex);

    return pushed;
}

bool
sc_aoa_push_open(struct sc_aoa *aoa, const struct sc_hid_open *hid_open,
                 bool exit_on_open_error) {
    if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
        sc_hid_open_log(hid_open);
    }

    sc_mutex_lock(&aoa->mutex);
    bool was_empty = sc_vecdeque_is_empty(&aoa->queue);

    // an OPEN event is non-droppable, so push it to the queue even above the
    // SC_AOA_EVENT_QUEUE_LIMIT
    struct sc_aoa_event *aoa_event = sc_vecdeque_push_hole(&aoa->queue);
    if (!aoa_event) {
        LOG_OOM();
        sc_mutex_unlock(&aoa->mutex);
        return false;
    }

    aoa_event->type = SC_AOA_EVENT_TYPE_OPEN;
    aoa_event->open.hid = *hid_open;
    aoa_event->open.exit_on_error = exit_on_open_error;

    if (was_empty) {
        sc_cond_signal(&aoa->event_cond);
    }

    sc_mutex_unlock(&aoa->mutex);

    return true;
}

bool
sc_aoa_push_close(struct sc_aoa *aoa, const struct sc_hid_close *hid_close) {
    if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
        sc_hid_close_log(hid_close);
    }

    sc_mutex_lock(&aoa->mutex);
    bool was_empty = sc_vecdeque_is_empty(&aoa->queue);

    // a CLOSE event is non-droppable, so push it to the queue even above the
    // SC_AOA_EVENT_QUEUE_LIMIT
    struct sc_aoa_event *aoa_event = sc_vecdeque_push_hole(&aoa->queue);
    if (!aoa_event) {
        LOG_OOM();
        sc_mutex_unlock(&aoa->mutex);
        return false;
    }

    aoa_event->type = SC_AOA_EVENT_TYPE_CLOSE;
    aoa_event->close.hid = *hid_close;

    if (was_empty) {
        sc_cond_signal(&aoa->event_cond);
    }

    sc_mutex_unlock(&aoa->mutex);

    return true;
}

static bool
sc_aoa_process_event(struct sc_aoa *aoa, struct sc_aoa_event *event,
                     struct sc_vec_hid_ids *vec_open) {
    switch (event->type) {
        case SC_AOA_EVENT_TYPE_INPUT: {
            uint64_t ack_to_wait = event->input.ack_to_wait;
            if (ack_to_wait != SC_SEQUENCE_INVALID) {
                LOGD("Waiting ack from server sequence=%" PRIu64_, ack_to_wait);

                // If some events have ack_to_wait set, then sc_aoa must have
                // been initialized with a non NULL acksync
                assert(aoa->acksync);

                // Do not block the loop indefinitely if the ack never comes (it
                // should never happen)
                sc_tick deadline = sc_tick_now() + SC_TICK_FROM_MS(500);
                enum sc_acksync_wait_result result =
                    sc_acksync_wait(aoa->acksync, ack_to_wait, deadline);

                if (result == SC_ACKSYNC_WAIT_TIMEOUT) {
                    LOGW("Ack not received after 500ms, discarding HID event");
                    // continue to process events
                    return true;
                } else if (result == SC_ACKSYNC_WAIT_INTR) {
                    // stopped
                    return false;
                }
            }

            struct sc_hid_input *hid_input = &event->input.hid;
            bool ok = sc_aoa_send_hid_event(aoa, hid_input);
            if (!ok) {
                LOGW("Could not send HID event to USB device: %" PRIu16,
                     hid_input->hid_id);
            }

            break;
        }
        case SC_AOA_EVENT_TYPE_OPEN: {
            struct sc_hid_open *hid_open = &event->open.hid;
            bool ok = sc_aoa_setup_hid(aoa, hid_open->hid_id,
                                       hid_open->report_desc,
                                       hid_open->report_desc_size);
            if (ok) {
                // The device is now open, add it to the list of devices to
                // close automatically on exit
                bool pushed = sc_vector_push(vec_open, hid_open->hid_id);
                if (!pushed) {
                    LOG_OOM();
                    // this is not fatal, the HID device will just not be
                    // explicitly unregistered
                }
            } else {
                LOGW("Could not open AOA device: %" PRIu16, hid_open->hid_id);
                if (event->open.exit_on_error) {
                    // Notify the error to the main thread, which will exit
                    sc_push_event(SC_EVENT_AOA_OPEN_ERROR);
                }
            }

            break;
        }
        case SC_AOA_EVENT_TYPE_CLOSE: {
            struct sc_hid_close *hid_close = &event->close.hid;
            bool ok = sc_aoa_unregister_hid(aoa, hid_close->hid_id);
            if (ok) {
                // The device is not open anymore, remove it from the list of
                // devices to close automatically on exit
                ssize_t idx = sc_vector_index_of(vec_open, hid_close->hid_id);
                if (idx >= 0) {
                    sc_vector_remove(vec_open, idx);
                }
            } else {
                LOGW("Could not close AOA device: %" PRIu16, hid_close->hid_id);
            }

            break;
        }
    }

    // continue to process events
    return true;
}

static int
run_aoa_thread(void *data) {
    struct sc_aoa *aoa = data;

    // Store the HID ids of opened devices to unregister them all before exiting
    struct sc_vec_hid_ids vec_open = SC_VECTOR_INITIALIZER;

    for (;;) {
        sc_mutex_lock(&aoa->mutex);
        while (!aoa->stopped && sc_vecdeque_is_empty(&aoa->queue)) {
            sc_cond_wait(&aoa->event_cond, &aoa->mutex);
        }
        if (aoa->stopped) {
            // Stop immediately, do not process further events
            sc_mutex_unlock(&aoa->mutex);
            break;
        }

        assert(!sc_vecdeque_is_empty(&aoa->queue));
        struct sc_aoa_event event = sc_vecdeque_pop(&aoa->queue);
        sc_mutex_unlock(&aoa->mutex);

        bool cont = sc_aoa_process_event(aoa, &event, &vec_open);
        if (!cont) {
            // stopped
            break;
        }
    }

    // Explicitly unregister all registered HID ids before exiting
    for (size_t i = 0; i < vec_open.size; ++i) {
        uint16_t hid_id = vec_open.data[i];
        LOGD("Unregistering AOA device %" PRIu16 "...", hid_id);
        bool ok = sc_aoa_unregister_hid(aoa, hid_id);
        if (!ok) {
            LOGW("Could not close AOA device: %" PRIu16, hid_id);
        }
    }
    sc_vector_destroy(&vec_open);

    return 0;
}

bool
sc_aoa_start(struct sc_aoa *aoa) {
    LOGD("Starting AOA thread");

    bool ok = sc_thread_create(&aoa->thread, run_aoa_thread, "scrcpy-aoa", aoa);
    if (!ok) {
        LOGE("Could not start AOA thread");
        return false;
    }

    return true;
}

void
sc_aoa_stop(struct sc_aoa *aoa) {
    sc_mutex_lock(&aoa->mutex);
    aoa->stopped = true;
    sc_cond_signal(&aoa->event_cond);
    sc_mutex_unlock(&aoa->mutex);

    if (aoa->acksync) {
        sc_acksync_interrupt(aoa->acksync);
    }
}

void
sc_aoa_join(struct sc_aoa *aoa) {
    sc_thread_join(&aoa->thread, NULL);
}
