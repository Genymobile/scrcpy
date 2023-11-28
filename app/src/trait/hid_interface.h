#ifndef SC_KEY_HID_INTERFACE_H
#define SC_KEY_HID_INTERFACE_H

#include "common.h"
#include "hid/hid_event.h"

struct sc_hid_interface {
    void
    (*process_output)(const uint8_t *data, size_t len, void *data_opaque);

    void *data_opaque;

    bool async_message; // Whether this HID interface transfers requests using Scrcpy control protocol or not

    const struct sc_hid_interface_ops *ops;
};

struct sc_hid_interface_ops {
    bool
    (*create)(struct sc_hid_interface *hid_interface, uint16_t id,
             uint16_t vendor_id, uint16_t product_id,
             const uint8_t* report_desc,
             uint16_t report_desc_size);

    bool
    (*process_input)(struct sc_hid_interface *hid_interface, const struct sc_hid_event* event);

    bool
    (*destroy)(struct sc_hid_interface *hid_interface, uint16_t id);
};

#endif
