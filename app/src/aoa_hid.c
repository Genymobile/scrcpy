#include "util/log.h"

#include <assert.h>

#include "aoa_hid.h"

// See <https://source.android.com/devices/accessories/aoa2#hid-support>.
#define ACCESSORY_REGISTER_HID 54
#define ACCESSORY_SET_HID_REPORT_DESC 56
#define ACCESSORY_SEND_HID_EVENT 57
#define ACCESSORY_UNREGISTER_HID 55

#define DEFAULT_TIMEOUT 1000

static void
hid_event_log(const struct hid_event *event) {
    // HID Event: [00] FF FF FF FF...
    assert(event->size);
    unsigned buffer_size = event->size * 3 + 1;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        return;
    }
    for (unsigned i = 0; i < event->size; ++i) {
        snprintf(buffer + i * 3, 4, " %02x", event->buffer[i]);
    }
    LOGV("HID Event: [%d]%s", event->from_accessory_id, buffer);
    free(buffer);
}

static void
hid_event_destroy(struct hid_event *event) {
    free(event->buffer);
}

static void
log_libusb_error(enum libusb_error errcode) {
    LOGW("libusb error: %s", libusb_strerror(errcode));
}

static bool
accept_device(libusb_device *device, const char *serial) {
    // do not log any USB error in this function, it is expected that many USB
    // devices available on the computer have permission restrictions

    struct libusb_device_descriptor desc;
    libusb_get_device_descriptor(device, &desc);

    if (!desc.iSerialNumber) {
        return false;
    }

    libusb_device_handle *handle;
    int result = libusb_open(device, &handle);
    if (result < 0) {
        return false;
    }

    char buffer[128];
    result = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                                                (unsigned char *) buffer,
                                                sizeof(buffer));
    libusb_close(handle);
    if (result < 0) {
        return false;
    }

    buffer[sizeof(buffer) - 1] = '\0'; // just in case

    // accept the device if its serial matches
    return !strcmp(buffer, serial);
}

static libusb_device *
aoa_find_usb_device(const char *serial) {
    if (!serial) {
        return NULL;
    }

    libusb_device **list;
    libusb_device *result = NULL;
    ssize_t count = libusb_get_device_list(NULL, &list);
    if (count < 0) {
        log_libusb_error((enum libusb_error) count);
        return NULL;
    }

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device *device = list[i];

        if (accept_device(device, serial)) {
            result = libusb_ref_device(device);
            break;
        }
    }
    libusb_free_device_list(list, 1);
    return result;
}

static int
aoa_open_usb_handle(libusb_device *device, libusb_device_handle **handle) {
    int result = libusb_open(device, handle);
    if (result < 0) {
        log_libusb_error((enum libusb_error) result);
        return result;
    }
    return 0;
}

bool
aoa_init(struct aoa *aoa, const char *serial) {
    cbuf_init(&aoa->queue);

    if (!sc_mutex_init(&aoa->mutex)) {
        return false;
    }

    if (!sc_cond_init(&aoa->event_cond)) {
        sc_mutex_destroy(&aoa->mutex);
        return false;
    }

    if (libusb_init(&aoa->usb_context) != LIBUSB_SUCCESS) {
        sc_cond_destroy(&aoa->event_cond);
        sc_mutex_destroy(&aoa->mutex);
        return false;
    }

    aoa->usb_device = aoa_find_usb_device(serial);
    if (!aoa->usb_device) {
        LOGW("USB device of serial %s not found", serial);
        libusb_exit(aoa->usb_context);
        sc_mutex_destroy(&aoa->mutex);
        sc_cond_destroy(&aoa->event_cond);
        return false;
    }

    if (aoa_open_usb_handle(aoa->usb_device, &aoa->usb_handle) < 0) {
        LOGW("Open USB handle failed");
        libusb_unref_device(aoa->usb_device);
        libusb_exit(aoa->usb_context);
        sc_cond_destroy(&aoa->event_cond);
        sc_mutex_destroy(&aoa->mutex);
        return false;
    }

    aoa->stopped = false;

    return true;
}

void
aoa_destroy(struct aoa *aoa) {
    // FIXME destroy all remaining HID events (cf controller_destroy())
    libusb_close(aoa->usb_handle);
    libusb_unref_device(aoa->usb_device);
    libusb_exit(aoa->usb_context);
    sc_cond_destroy(&aoa->event_cond);
    sc_mutex_destroy(&aoa->mutex);
}

static bool
aoa_register_hid(struct aoa *aoa, uint16_t accessory_id,
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
    int result = libusb_control_transfer(aoa->usb_handle, request_type, request,
                                         value, index, buffer, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        log_libusb_error((enum libusb_error) result);
        return false;
    }

    return true;
}

static bool
aoa_set_hid_report_desc(struct aoa *aoa, uint16_t accessory_id,
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
    int result = libusb_control_transfer(aoa->usb_handle, request_type, request,
                                         value, index, buffer, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        log_libusb_error((enum libusb_error) result);
        return false;
    }

    return true;
}

bool
aoa_setup_hid(struct aoa *aoa, uint16_t accessory_id,
              const unsigned char *report_desc, uint16_t report_desc_size) {
    bool ok = aoa_register_hid(aoa, accessory_id, report_desc_size);
    if (!ok) {
        return false;
    }

    ok = aoa_set_hid_report_desc(aoa, accessory_id, report_desc,
                                 report_desc_size);
    if (!ok) {
        if (!aoa_unregister_hid(aoa, accessory_id)) {
            LOGW("Could not unregister HID");
        }
        return false;
    }

    return true;
}

static bool
aoa_send_hid_event(struct aoa *aoa, const struct hid_event *event) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_SEND_HID_EVENT;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): 0 (unused)
    uint16_t value = event->from_accessory_id;
    uint16_t index = 0;
    unsigned char *buffer = event->buffer;
    uint16_t length = event->size;
    int result = libusb_control_transfer(aoa->usb_handle, request_type, request,
                                         value, index, buffer, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        log_libusb_error((enum libusb_error) result);
        return false;
    }

    return true;
}

bool
aoa_unregister_hid(struct aoa *aoa, const uint16_t accessory_id) {
    uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR;
    uint8_t request = ACCESSORY_UNREGISTER_HID;
    // <https://source.android.com/devices/accessories/aoa2.html#hid-support>
    // value (arg0): accessory assigned ID for the HID device
    // index (arg1): 0
    uint16_t value = accessory_id;
    uint16_t index = 0;
    unsigned char *buffer = NULL;
    uint16_t length = 0;
    int result = libusb_control_transfer(aoa->usb_handle, request_type, request,
                                         value, index, buffer, length,
                                         DEFAULT_TIMEOUT);
    if (result < 0) {
        log_libusb_error((enum libusb_error) result);
        return false;
    }

    return true;
}

bool
aoa_push_hid_event(struct aoa *aoa, const struct hid_event *event) {
    hid_event_log(event);
    sc_mutex_lock(&aoa->mutex);
    bool was_empty = cbuf_is_empty(&aoa->queue);
    bool res = cbuf_push(&aoa->queue, *event);
    if (was_empty) {
        sc_cond_signal(&aoa->event_cond);
    }
    sc_mutex_unlock(&aoa->mutex);
    return res;
}

static inline bool
process_hid_event(struct aoa *aoa, const struct hid_event *event) {
    return aoa_send_hid_event(aoa, event);
}

static int
run_aoa_thread(void *data) {
    struct aoa *aoa = data;

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
        struct hid_event event;
        bool non_empty = cbuf_take(&aoa->queue, &event);
        assert(non_empty);
        (void) non_empty;
        sc_mutex_unlock(&aoa->mutex);

        bool ok = process_hid_event(aoa, &event);
        hid_event_destroy(&event);
        if (!ok) {
            LOGW("Could not send HID event to USB device");
        }
    }
    return 0;
}

bool
aoa_start(struct aoa *aoa) {
    LOGD("Starting AOA thread");

    bool ok = sc_thread_create(&aoa->thread, run_aoa_thread, "aoa_thread", aoa);
    if (!ok) {
        LOGC("Could not start AOA thread");
        return false;
    }

    return true;
}

void
aoa_stop(struct aoa *aoa) {
    sc_mutex_lock(&aoa->mutex);
    aoa->stopped = true;
    sc_cond_signal(&aoa->event_cond);
    sc_mutex_unlock(&aoa->mutex);
}

void
aoa_join(struct aoa *aoa) {
    sc_thread_join(&aoa->thread, NULL);
}
