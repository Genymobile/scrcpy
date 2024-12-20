#ifndef SC_HID_EVENT_H
#define SC_HID_EVENT_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>

#define SC_HID_MAX_SIZE 15

struct sc_hid_input {
    uint16_t hid_id;
    uint8_t data[SC_HID_MAX_SIZE];
    uint8_t size;
};

struct sc_hid_open {
    uint16_t hid_id;
    const uint8_t *report_desc; // pointer to static memory
    size_t report_desc_size;
};

struct sc_hid_close {
    uint16_t hid_id;
};

#endif
