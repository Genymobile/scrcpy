#include "util/log.h"

#include <assert.h>
#include <stdio.h>

#include "aoa_hid.h"
#include "util/log.h"

// See <https://source.android.com/devices/accessories/aoa2#hid-support>.
#define ACCESSORY_REGISTER_HID 54
#define ACCESSORY_SET_HID_REPORT_DESC 56
#define ACCESSORY_SEND_HID_EVENT 57
#define ACCESSORY_UNREGISTER_HID 55

#define DEFAULT_TIMEOUT 1000

static void
sc_hid_event_log(const struct sc_hid_event *event) {
    // HID Event: [00] FF FF FF FF...
    assert(event->size);
    unsigned buffer_size = event->size * 3 + 1;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        LOG_OOM();
        return;
    }
    for (unsigned i = 0; i < event->size; ++i) {
        snprintf(buffer + i * 3, 4, " %02x", event->buffer[i]);
    }
    LOGV("HID Event: [%d]%s", event->accessory_id, buffer);
    free(buffer);
}

void
sc_hid_event_init(struct sc_hid_event *hid_event, uint16_t accessory_id,
                  unsigned char *buffer, uint16_t buffer_size) {
    hid_event->accessory_id = accessory_id;
    hid_event->buffer = buffer;
    hid_event->size = buffer_size;
    hid_event->ack_to_wait = SC_SEQUENCE_INVALID;
}

void
sc_hid_event_destroy(struct sc_hid_event *hid_event) {
    free(hid_event->buffer);
}

bool
sc_aoa_init(struct sc_aoa *aoa, struct sc_usb *usb,
            struct sc_acksync *acksync) {
    cbuf_init(&aoa->queue);

    if (!sc_mutex_init(&aoa->mutex)) {
        return false;
    }

    if (!sc_cond_init(&aoa->event_cond)) {
        sc_mutex_destroy(&aoa->mutex);
        return false;
    }

    aoa->stopped = false;
    aoa->acksync = acksync;
    aoa->usb = usb;

    return true;
}

void
sc_aoa_destroy(struct sc_aoa *aoa) {
    // Destroy remaining events
    struct sc_hid_event event;
    while (cbuf_take(&aoa->queue, &event)) {
        sc_hid_event_destroy(&event);
    }

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
    unsigned char *buffer = NULL;
    uint16_t length = 0;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, buffer, length,
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
                           const unsigned char *report_desc,
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
    // index (arg1): offset of data (buffer) in descriptor
    uint16_t value = accessory_id;
    uint16_t index = 0;
    // libusb_control_transfer expects a pointer to non-const
    unsigned char *buffer = (unsigned char *) report_desc;
    uint16_t length = report_desc_size;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, buffer, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        LOGE("SET_HID_REPORT_DESC: libusb error: %s", libusb_strerror(result));
        sc_usb_check_disconnected(aoa->usb, result);
        return false;
    }

    return true;
}

bool
sc_aoa_setup_hid(struct sc_aoa *aoa, uint16_t accessory_id,
                 const unsigned char *report_desc, uint16_t report_desc_size) {
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

static bool
sc_aoa_send_hid_event(struct sc_aoa *aoa, const struct sc_hid_event *event) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_SEND_HID_EVENT;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): 0 (unused)
    uint16_t value = event->accessory_id;
    uint16_t index = 0;
    unsigned char *buffer = event->buffer;
    uint16_t length = event->size;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, buffer, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        LOGE("SEND_HID_EVENT: libusb error: %s", libusb_strerror(result));
        sc_usb_check_disconnected(aoa->usb, result);
        return false;
    }

    return true;
}

bool
sc_aoa_unregister_hid(struct sc_aoa *aoa, const uint16_t accessory_id) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_UNREGISTER_HID;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): 0
    uint16_t value = accessory_id;
    uint16_t index = 0;
    unsigned char *buffer = NULL;
    uint16_t length = 0;
    int result = libusb_control_transfer(aoa->usb->handle, request_type,
                                         request, value, index, buffer, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        LOGE("UNREGISTER_HID: libusb error: %s", libusb_strerror(result));
        sc_usb_check_disconnected(aoa->usb, result);
        return false;
    }

    return true;
}

bool
sc_aoa_push_hid_event(struct sc_aoa *aoa, const struct sc_hid_event *event) {
    if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
        sc_hid_event_log(event);
    }

    sc_mutex_lock(&aoa->mutex);
    bool was_empty = cbuf_is_empty(&aoa->queue);
    bool res = cbuf_push(&aoa->queue, *event);
    if (was_empty) {
        sc_cond_signal(&aoa->event_cond);
    }
    sc_mutex_unlock(&aoa->mutex);
    return res;
}

static int
run_aoa_thread(void *data) {
    struct sc_aoa *aoa = data;

    for (;;) {
        sc_mutex_lock(&aoa->mutex);
        while (!aoa->stopped && cbuf_is_empty(&aoa->queue)) {
            sc_cond_wait(&aoa->event_cond, &aoa->mutex);
        }
        if (aoa->stopped) {
            // Stop immediately, do not process further events
            sc_mutex_unlock(&aoa->mutex);
            break;
        }
        struct sc_hid_event event;
        bool non_empty = cbuf_take(&aoa->queue, &event);
        assert(non_empty);
        (void) non_empty;

        uint64_t ack_to_wait = event.ack_to_wait;
        sc_mutex_unlock(&aoa->mutex);

        if (ack_to_wait != SC_SEQUENCE_INVALID) {
            LOGD("Waiting ack from server sequence=%" PRIu64_, ack_to_wait);

            // If some events have ack_to_wait set, then sc_aoa must have been
            // initialized with a non NULL acksync
            assert(aoa->acksync);

            // Do not block the loop indefinitely if the ack never comes (it
            // should never happen)
            sc_tick deadline = sc_tick_now() + SC_TICK_FROM_MS(500);
            enum sc_acksync_wait_result result =
                sc_acksync_wait(aoa->acksync, ack_to_wait, deadline);

            if (result == SC_ACKSYNC_WAIT_TIMEOUT) {
                LOGW("Ack not received after 500ms, discarding HID event");
                sc_hid_event_destroy(&event);
                continue;
            } else if (result == SC_ACKSYNC_WAIT_INTR) {
                // stopped
                sc_hid_event_destroy(&event);
                break;
            }
        }

        bool ok = sc_aoa_send_hid_event(aoa, &event);
        sc_hid_event_destroy(&event);
        if (!ok) {
            LOGW("Could not send HID event to USB device");
        }
    }
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
