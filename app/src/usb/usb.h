#ifndef SC_USB_H
#define SC_USB_H

#include "common.h"

#include <stdbool.h>
#include <libusb-1.0/libusb.h>

struct sc_usb {
    libusb_context *context;
    libusb_device_handle *handle;
};

struct sc_usb_device {
    libusb_device *device;
    char *serial;
    char *manufacturer;
    char *product;
    uint16_t vid;
    uint16_t pid;
};

void
sc_usb_device_destroy(struct sc_usb_device *usb_device);

bool
sc_usb_init(struct sc_usb *usb);

void
sc_usb_destroy(struct sc_usb *usb);

bool
sc_usb_find_device(struct sc_usb *usb, const char *serial,
                   struct sc_usb_device *out);

bool
sc_usb_connect(struct sc_usb *usb, libusb_device *device);

void
sc_usb_disconnect(struct sc_usb *usb);

#endif
