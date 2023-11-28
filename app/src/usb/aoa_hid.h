#ifndef SC_AOA_HID_H
#define SC_AOA_HID_H

#include <stdint.h>
#include <stdbool.h>

#include <libusb-1.0/libusb.h>

#include "usb.h"
#include "trait/hid_interface.h"
#include "util/acksync.h"
#include "util/thread.h"
#include "util/tick.h"
#include "util/vecdeque.h"

struct sc_hid_event_queue SC_VECDEQUE(struct sc_hid_event);

struct sc_aoa {
    struct sc_hid_interface hid_interface;

    struct sc_usb *usb;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond event_cond;
    bool stopped;
    struct sc_hid_event_queue queue;

    struct sc_acksync *acksync;
};

bool
sc_aoa_init(struct sc_aoa *aoa, struct sc_usb *usb, struct sc_acksync *acksync);

void
sc_aoa_destroy(struct sc_aoa *aoa);

bool
sc_aoa_start(struct sc_aoa *aoa);

void
sc_aoa_stop(struct sc_aoa *aoa);

void
sc_aoa_join(struct sc_aoa *aoa);

#endif
