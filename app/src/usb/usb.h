#ifndef SC_USB_H
#define SC_USB_H

#include "common.h"

#include <stdbool.h>
#include <libusb-1.0/libusb.h>

#include "util/thread.h"

struct sc_usb {
    libusb_context *context;
    libusb_device_handle *handle;

    const struct sc_usb_callbacks *cbs;
    void *cbs_userdata;

    bool has_callback_handle;
    libusb_hotplug_callback_handle callback_handle;

    bool has_libusb_event_thread;
    sc_thread libusb_event_thread;

    atomic_bool stopped; // only used if cbs != NULL
    atomic_flag disconnection_notified;
};

struct sc_usb_callbacks {
    void (*on_disconnected)(struct sc_usb *usb, void *userdata);
};

struct sc_usb_device {
    libusb_device *device;
    char *serial;
    char *manufacturer;
    char *product;
    uint16_t vid;
    uint16_t pid;
    bool selected;
};

void
sc_usb_device_destroy(struct sc_usb_device *usb_device);

/**
 * Move src to dst
 *
 * After this call, the content of src is undefined, except that
 * sc_usb_device_destroy() can be called.
 *
 * This is useful to take a device from a list that will be destroyed, without
 * making unnecessary copies.
 */
void
sc_usb_device_move(struct sc_usb_device *dst, struct sc_usb_device *src);

void
sc_usb_devices_destroy_all(struct sc_usb_device *usb_devices, size_t count);

bool
sc_usb_init(struct sc_usb *usb);

void
sc_usb_destroy(struct sc_usb *usb);

bool
sc_usb_select_device(struct sc_usb *usb, const char *serial,
                     struct sc_usb_device *out_device);

bool
sc_usb_connect(struct sc_usb *usb, libusb_device *device,
               const struct sc_usb_callbacks *cbs, void *cbs_userdata);

void
sc_usb_disconnect(struct sc_usb *usb);

// A client should call this function with the return value of a libusb call
// to detect disconnection immediately
bool
sc_usb_check_disconnected(struct sc_usb *usb, int result);

void
sc_usb_stop(struct sc_usb *usb);

void
sc_usb_join(struct sc_usb *usb);

#endif
