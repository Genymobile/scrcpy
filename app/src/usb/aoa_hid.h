#ifndef SC_AOA_HID_H
#define SC_AOA_HID_H

#include <stdint.h>
#include <stdbool.h>

#include <libusb-1.0/libusb.h>

#include "usb.h"
#include "util/acksync.h"
#include "util/cbuf.h"
#include "util/thread.h"
#include "util/tick.h"

struct sc_hid_event {
    uint16_t accessory_id;
    unsigned char *buffer;
    uint16_t size;
    uint64_t ack_to_wait;
};

// Takes ownership of buffer
void
sc_hid_event_init(struct sc_hid_event *hid_event, uint16_t accessory_id,
                  unsigned char *buffer, uint16_t buffer_size);

void
sc_hid_event_destroy(struct sc_hid_event *hid_event);

struct sc_hid_event_queue CBUF(struct sc_hid_event, 64);

struct sc_aoa {
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

bool
sc_aoa_setup_hid(struct sc_aoa *aoa, uint16_t accessory_id,
              const unsigned char *report_desc, uint16_t report_desc_size);

bool
sc_aoa_unregister_hid(struct sc_aoa *aoa, uint16_t accessory_id);

bool
sc_aoa_push_hid_event(struct sc_aoa *aoa, const struct sc_hid_event *event);

#endif
