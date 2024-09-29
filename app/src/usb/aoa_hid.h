#ifndef SC_AOA_HID_H
#define SC_AOA_HID_H

#include "common.h"

#include <stdint.h>
#include <stdbool.h>

#include <libusb-1.0/libusb.h>

#include "hid/hid_event.h"
#include "usb.h"
#include "util/acksync.h"
#include "util/thread.h"
#include "util/tick.h"
#include "util/vecdeque.h"

enum sc_aoa_event_type {
    SC_AOA_EVENT_TYPE_OPEN,
    SC_AOA_EVENT_TYPE_INPUT,
    SC_AOA_EVENT_TYPE_CLOSE,
};

struct sc_aoa_event {
    enum sc_aoa_event_type type;
    union {
        struct {
            struct sc_hid_open hid;
            bool exit_on_error;
        } open;
        struct {
            struct sc_hid_close hid;
        } close;
        struct {
            struct sc_hid_input hid;
            uint64_t ack_to_wait;
        } input;
    };
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

//bool
//sc_aoa_setup_hid(struct sc_aoa *aoa, uint16_t accessory_id,
//              const uint8_t *report_desc, uint16_t report_desc_size);
//
//bool
//sc_aoa_unregister_hid(struct sc_aoa *aoa, uint16_t accessory_id);

// report_desc must be a pointer to static memory, accessed at any time from
// another thread
bool
sc_aoa_push_open(struct sc_aoa *aoa, const struct sc_hid_open *hid_open,
                 bool exit_on_open_error);

bool
sc_aoa_push_close(struct sc_aoa *aoa, const struct sc_hid_close *hid_close);

bool
sc_aoa_push_input_with_ack_to_wait(struct sc_aoa *aoa,
                                   const struct sc_hid_input *hid_input,
                                   uint64_t ack_to_wait);

static inline bool
sc_aoa_push_input(struct sc_aoa *aoa, const struct sc_hid_input *hid_input) {
    return sc_aoa_push_input_with_ack_to_wait(aoa, hid_input,
                                              SC_SEQUENCE_INVALID);
}

#endif
