#ifndef AOA_HID_H
#define AOA_HID_H

#include <stdint.h>
#include <stdbool.h>

#include <libusb-1.0/libusb.h>

#include "scrcpy.h"
#include "util/cbuf.h"
#include "util/thread.h"
#include "util/tick.h"

struct hid_event {
    uint16_t from_accessory_id;
    unsigned char *buffer;
    uint16_t size;
    sc_tick delay;
};

// Takes ownership of buffer
void
hid_event_init(struct hid_event *hid_event, uint16_t accessory_id,
               unsigned char *buffer, uint16_t buffer_size);

void
hid_event_destroy(struct hid_event *hid_event);

struct hid_event_queue CBUF(struct hid_event, 64);

struct aoa {
    libusb_context *usb_context;
    libusb_device *usb_device;
    libusb_device_handle *usb_handle;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond event_cond;
    bool stopped;
    struct hid_event_queue queue;
};

bool
aoa_init(struct aoa *aoa, const char *serial);

void
aoa_destroy(struct aoa *aoa);

bool
aoa_start(struct aoa *aoa);

void
aoa_stop(struct aoa *aoa);

void
aoa_join(struct aoa *aoa);

bool
aoa_setup_hid(struct aoa *aoa, uint16_t accessory_id,
              const unsigned char *report_desc, uint16_t report_desc_size);

bool
aoa_unregister_hid(struct aoa *aoa, uint16_t accessory_id);

bool
aoa_push_hid_event(struct aoa *aoa, const struct hid_event *event);

#endif
