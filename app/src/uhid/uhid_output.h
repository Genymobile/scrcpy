#ifndef SC_UHID_OUTPUT_H
#define SC_UHID_OUTPUT_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>

/**
 * The communication with UHID devices is bidirectional.
 *
 * This component dispatches HID outputs to the expected processor.
 */

struct sc_uhid_devices {
    struct sc_keyboard_uhid *keyboard;
};

void
sc_uhid_devices_init(struct sc_uhid_devices *devices,
                     struct sc_keyboard_uhid *keyboard);

void
sc_uhid_devices_process_hid_output(struct sc_uhid_devices *devices, uint16_t id,
                                   const uint8_t *data, size_t size);

#endif
