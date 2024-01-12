#ifndef SC_KEY_HID_INTERFACE_H
#define SC_KEY_HID_INTERFACE_H

#include "common.h"

#include <stdbool.h>

#include "hid/hid_event.h"

struct sc_hid_device {
    uint16_t id;

    void
    (*process_output)(struct sc_hid_device *hid_device, int16_t report_id, const uint8_t *data,
                      size_t size);
};

struct sc_hid_interface {
    // Whether this HID interface transfers requests using Scrcpy control protocol or not
    bool async_message;
    // Whether this HID interface supports output reports
    bool support_output;

    // A dynamic growing array of devices
    struct sc_hid_device **devices;
    size_t devices_count;
    size_t devices_capacity;

    const struct sc_hid_interface_ops *ops;
};

struct sc_hid_interface_ops {
    bool
    (*create)(struct sc_hid_interface *hid_interface, struct sc_hid_device* device,
              uint16_t vendor_id, uint16_t product_id, const uint8_t* report_desc,
              uint16_t report_desc_size);

    bool
    (*process_input)(struct sc_hid_interface *hid_interface, const struct sc_hid_event* event);

    bool
    (*destroy)(struct sc_hid_interface *hid_interface, uint16_t id);
};

void
sc_hid_interface_init(struct sc_hid_interface *hid_interface);

void
sc_hid_interface_add_device(struct sc_hid_interface *hid_interface, struct sc_hid_device *device);

struct sc_hid_device*
sc_hid_interface_get_device(struct sc_hid_interface *hid_interface, uint16_t id);

void
sc_hid_interface_remove_device(struct sc_hid_interface *hid_interface, int16_t id);

#endif
