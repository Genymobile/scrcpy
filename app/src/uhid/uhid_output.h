#ifndef SC_UHID_OUTPUT_H
#define SC_UHID_OUTPUT_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * The communication with UHID devices is bidirectional.
 *
 * This component manages the registration of receivers to handle UHID output
 * messages (sent from the device to the computer).
 */

struct sc_uhid_receiver {
    uint16_t id;

    const struct sc_uhid_receiver_ops *ops;
};

struct sc_uhid_receiver_ops {
    void
    (*process_output)(struct sc_uhid_receiver *receiver,
                      const uint8_t *data, size_t len);
};

#define SC_UHID_MAX_RECEIVERS 1

struct sc_uhid_devices {
    struct sc_uhid_receiver *receivers[SC_UHID_MAX_RECEIVERS];
    unsigned count;
};

void
sc_uhid_devices_init(struct sc_uhid_devices *devices);

void
sc_uhid_devices_add_receiver(struct sc_uhid_devices *devices,
                             struct sc_uhid_receiver *receiver);

struct sc_uhid_receiver *
sc_uhid_devices_get_receiver(struct sc_uhid_devices *devices, uint16_t id);

#endif
