#ifndef SC_UHID_H
#define SC_UHID_H

#include "common.h"
#include "controller.h"
#include "device_msg.h"
#include "hid/hid_interface.h"

struct sc_uhid {
    struct sc_hid_interface hid_interface;

    struct sc_controller *controller;
};

void
sc_uhid_init(struct sc_uhid *uhid, struct sc_controller *controller);

void
sc_uhid_process_output(struct sc_uhid *uhid, uint32_t id, const uint8_t *data, uint32_t size);

#endif
