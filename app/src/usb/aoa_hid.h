#ifndef SC_AOA_HID_H
#define SC_AOA_HID_H

#include <stdint.h>
#include <stdbool.h>

#include <libusb-1.0/libusb.h>

#include "hid/hid_event.h"
#include "usb.h"
#include "util/acksync.h"
#include "util/thread.h"
#include "util/tick.h"
#include "util/vecdeque.h"

#define SC_HID_MAX_SIZE 8

struct sc_aoa_event {
    struct sc_hid_event hid;
    uint16_t accessory_id;
    uint64_t ack_to_wait;
};

struct sc_aoa_event_queue SC_VECDEQUE(struct sc_aoa_event);

struct sc_aoa {
    struct sc_usb *usb;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond event_cond;
    bool stopped;
    struct sc_aoa_event_queue queue;

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
              const uint8_t *report_desc, uint16_t report_desc_size);

bool
sc_aoa_unregister_hid(struct sc_aoa *aoa, uint16_t accessory_id);

bool
sc_aoa_push_hid_event_with_ack_to_wait(struct sc_aoa *aoa,
                                       uint16_t accessory_id,
                                       const struct sc_hid_event *event,
                                       uint64_t ack_to_wait);

static inline bool
sc_aoa_push_hid_event(struct sc_aoa *aoa, uint16_t accessory_id,
                      const struct sc_hid_event *event) {
    return sc_aoa_push_hid_event_with_ack_to_wait(aoa, accessory_id, event,
                                                  SC_SEQUENCE_INVALID);
}

#endif
