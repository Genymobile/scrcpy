#ifndef SC_HID_EVENT_H
#define SC_HID_EVENT_H

#include "common.h"

#include <stdint.h>

#define SC_HID_MAX_SIZE 8

struct sc_hid_event {
    uint8_t data[SC_HID_MAX_SIZE];
    uint8_t size;
};

#endif
