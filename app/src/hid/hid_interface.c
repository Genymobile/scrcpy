#include "hid_interface.h"

#include <stdlib.h>

#define SC_HID_DEVICE_INITIAL_CAPACITY 4

void
sc_hid_interface_init(struct sc_hid_interface *hid_interface) {
    hid_interface->async_message = false;
    hid_interface->support_output = false;

    hid_interface->devices = malloc(sizeof(struct sc_hid_device) * SC_HID_DEVICE_INITIAL_CAPACITY);
    hid_interface->devices_count = 0;
    hid_interface->devices_capacity = SC_HID_DEVICE_INITIAL_CAPACITY;
}

void
sc_hid_interface_add_device(struct sc_hid_interface *hid_interface, struct sc_hid_device *device) {
    if (hid_interface->devices_count == hid_interface->devices_capacity) {
        hid_interface->devices_capacity *= 2;
        hid_interface->devices = realloc(hid_interface->devices, sizeof(struct sc_hid_device) * hid_interface->devices_capacity);
    }
    hid_interface->devices[hid_interface->devices_count++] = device;
}

struct sc_hid_device*
sc_hid_interface_get_device(struct sc_hid_interface *hid_interface, uint16_t id) {
    for (size_t i = 0; i < hid_interface->devices_count; ++i) {
        if (hid_interface->devices[i]->id == id) {
            return hid_interface->devices[i];
        }
    }
    return NULL;
}

void
sc_hid_interface_remove_device(struct sc_hid_interface *hid_interface, int16_t id) {
    for (size_t i = 0; i < hid_interface->devices_count; ++i) {
        if (hid_interface->devices[i]->id == id) {
            // Swap with the last element
            hid_interface->devices[i] = hid_interface->devices[--hid_interface->devices_count];
            return;
        }
    }
}
