#include "usb.h"

#include <assert.h>

#include "util/log.h"

static inline void
log_libusb_error(enum libusb_error errcode) {
    LOGW("libusb error: %s", libusb_strerror(errcode));
}

static char *
read_string(libusb_device_handle *handle, uint8_t desc_index) {
    char buffer[128];
    int result =
        libusb_get_string_descriptor_ascii(handle, desc_index,
                                           (unsigned char *) buffer,
                                           sizeof(buffer));
    if (result < 0) {
        return NULL;
    }

    assert((size_t) result <= sizeof(buffer));

    // When non-negative, 'result' contains the number of bytes written
    char *s = malloc(result + 1);
    memcpy(s, buffer, result);
    s[result] = '\0';
    return s;
}

static bool
accept_device(libusb_device *device, const char *serial,
              struct sc_usb_device *out) {
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

    char *device_serial = read_string(handle, desc.iSerialNumber);
    if (!device_serial) {
        libusb_close(handle);
        return false;
    }

    if (serial) {
        // Filter by serial
        bool matches = !strcmp(serial, device_serial);
        if (!matches) {
            free(device_serial);
            libusb_close(handle);
            return false;
        }
    }

    out->device = libusb_ref_device(device);
    out->serial = device_serial;
    out->vid = desc.idVendor;
    out->pid = desc.idProduct;
    out->manufacturer = read_string(handle, desc.iManufacturer);
    out->product = read_string(handle, desc.iProduct);

    libusb_close(handle);

    return true;
}

void
sc_usb_device_destroy(struct sc_usb_device *usb_device) {
    libusb_unref_device(usb_device->device);
    free(usb_device->serial);
    free(usb_device->manufacturer);
    free(usb_device->product);
}

void
sc_usb_device_destroy_all(struct sc_usb_device *usb_devices, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        sc_usb_device_destroy(&usb_devices[i]);
    }
}

ssize_t
sc_usb_find_devices(struct sc_usb *usb, const char *serial,
                    struct sc_usb_device *devices, size_t len) {
    libusb_device **list;
    ssize_t count = libusb_get_device_list(usb->context, &list);
    if (count < 0) {
        log_libusb_error((enum libusb_error) count);
        return -1;
    }

    size_t idx = 0;
    for (size_t i = 0; i < (size_t) count && idx < len; ++i) {
        libusb_device *device = list[i];

        if (accept_device(device, serial, &devices[idx])) {
            ++idx;
        }
    }

    libusb_free_device_list(list, 1);
    return idx;
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
sc_usb_init(struct sc_usb *usb) {
    usb->handle = NULL;
    return libusb_init(&usb->context) == LIBUSB_SUCCESS;
}

void
sc_usb_destroy(struct sc_usb *usb) {
    libusb_exit(usb->context);
}

bool
sc_usb_connect(struct sc_usb *usb, libusb_device *device) {
    usb->handle = sc_usb_open_handle(device);
    if (!usb->handle) {
        return false;
    }

    return true;
}

void
sc_usb_disconnect(struct sc_usb *usb) {
    libusb_close(usb->handle);
}
