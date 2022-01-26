#include "usb.h"

#include <assert.h>

#include "util/log.h"

static inline void
log_libusb_error(enum libusb_error errcode) {
    LOGW("libusb error: %s", libusb_strerror(errcode));
}

static bool
accept_device(libusb_device *device, const char *serial) {
    // Do not log any USB error in this function, it is expected that many USB
    // devices available on the computer have permission restrictions

    struct libusb_device_descriptor desc;
    int result = libusb_get_device_descriptor(device, &desc);
    if (result < 0 || !desc.iSerialNumber) {
        return false;
    }

    libusb_device_handle *handle;
    result = libusb_open(device, &handle);
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

    // Accept the device if its serial matches
    return !strcmp(buffer, serial);
}

static libusb_device *
sc_usb_find_device(const char *serial) {
    libusb_device **list;
    libusb_device *result = NULL;
    ssize_t count = libusb_get_device_list(NULL, &list);
    if (count < 0) {
        log_libusb_error((enum libusb_error) count);
        return NULL;
    }

    for (size_t i = 0; i < (size_t) count; ++i) {
        libusb_device *device = list[i];

        if (accept_device(device, serial)) {
            result = libusb_ref_device(device);
            break;
        }
    }

    libusb_free_device_list(list, 1);
    return result;
}

static libusb_device_handle *
sc_usb_open_handle(libusb_device *device) {
    libusb_device_handle *handle;
    int result = libusb_open(device, &handle);
    if (result < 0) {
        log_libusb_error((enum libusb_error) result);
        return NULL;
    }
    return handle;
 }

bool
sc_usb_init(struct sc_usb *usb, const char *serial) {
    assert(serial);

    // There is only one device, initialize the context here
    if (libusb_init(&usb->context) != LIBUSB_SUCCESS) {
        return false;
    }

    libusb_device *device = sc_usb_find_device(serial);
    if (!device) {
        LOGW("USB device %s not found", serial);
        libusb_exit(usb->context);
        return false;
    }

    usb->handle = sc_usb_open_handle(device);
    libusb_unref_device(device);
    if (!usb->handle) {
        LOGW("Could not open USB device %s", serial);
        libusb_exit(usb->context);
        return false;
    }

    return true;
}

void
sc_usb_destroy(struct sc_usb *usb) {
    libusb_close(usb->handle);
    libusb_exit(usb->context);
}
