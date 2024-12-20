#include "uhid_output.h"

#include <inttypes.h>

#include "uhid/keyboard_uhid.h"
#include "util/log.h"

void
sc_uhid_devices_init(struct sc_uhid_devices *devices,
                     struct sc_keyboard_uhid *keyboard) {
    devices->keyboard = keyboard;
}

void
sc_uhid_devices_process_hid_output(struct sc_uhid_devices *devices, uint16_t id,
                                   const uint8_t *data, size_t size) {
    if (id == SC_HID_ID_KEYBOARD) {
        if (devices->keyboard) {
            sc_keyboard_uhid_process_hid_output(devices->keyboard, data, size);
        } else {
            LOGW("Unexpected keyboard HID output without UHID keyboard");
        }
    } else {
        LOGW("HID output ignored for id %" PRIu16, id);
    }
}
