#include "uhid_output.h"

#include <assert.h>

void
sc_uhid_devices_init(struct sc_uhid_devices *devices) {
    devices->count = 0;
}

void
sc_uhid_devices_add_receiver(struct sc_uhid_devices *devices,
                             struct sc_uhid_receiver *receiver) {
    assert(devices->count < SC_UHID_MAX_RECEIVERS);
    devices->receivers[devices->count++] = receiver;
}

struct sc_uhid_receiver *
sc_uhid_devices_get_receiver(struct sc_uhid_devices *devices, uint16_t id) {
    for (size_t i = 0; i < devices->count; ++i) {
        if (devices->receivers[i]->id == id) {
            return devices->receivers[i];
        }
    }
    return NULL;
}
