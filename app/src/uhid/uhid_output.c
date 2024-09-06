#include "uhid_output.h"

#include <assert.h>
#include <inttypes.h>

#include "uhid/keyboard_uhid.h"
#include "uhid/gamepad_uhid.h"
#include "util/log.h"

void
sc_uhid_devices_init(struct sc_uhid_devices *devices,
                     struct sc_keyboard_uhid *keyboard,
                     struct sc_gamepad_uhid *gamepad) {
    devices->keyboard = keyboard;
    devices->gamepad = gamepad;
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
    } else if (id >= SC_HID_ID_GAMEPAD_FIRST && id <= SC_HID_ID_GAMEPAD_LAST) {
        if (devices->gamepad) {
            sc_gamepad_uhid_process_hid_output(devices->gamepad, id, data,
                                               size);
        } else {
            LOGW("Unexpected gamepad HID output without UHID gamepad");
        }
    } else {
        LOGW("HID output ignored for id %" PRIu16, id);
    }
}
