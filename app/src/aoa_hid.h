#ifndef AOA_HID_H
#define AOA_HID_H

#include <stdint.h>
#include <stdbool.h>

#include <libusb-1.0/libusb.h>

#include "scrcpy.h"
#include "util/cbuf.h"
#include "util/thread.h"

struct report_desc {
    uint16_t from_accessory_id;
    unsigned char *buffer;
    uint16_t size;
};

struct hid_event {
    uint16_t from_accessory_id;
    unsigned char *buffer;
    uint16_t size;
};

struct hid_event_queue CBUF(struct hid_event, 64);

struct aoa {
    libusb_context *usb_context;
    libusb_device *usb_device;
    libusb_device_handle *usb_handle;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond event_cond;
    bool stopped;
    uint16_t next_accessories_id;
    struct hid_event_queue queue;
};

void hid_event_log(const struct hid_event *event);
void hid_event_destroy(struct hid_event *event);
bool aoa_init(struct aoa *aoa, const struct scrcpy_options *options);
// Generate a different accessory ID.
uint16_t aoa_get_new_accessory_id(struct aoa *aoa);
int
aoa_register_hid(struct aoa *aoa, const uint16_t accessory_id,
    uint16_t report_desc_size);
int
aoa_set_hid_report_desc(struct aoa *aoa, const struct report_desc *report_desc);
int
aoa_send_hid_event(struct aoa *aoa, const struct hid_event *event);
int aoa_unregister_hid(struct aoa *aoa, const uint16_t accessory_id);
bool aoa_push_hid_event(struct aoa *aoa, const struct hid_event *event);
bool aoa_thread_start(struct aoa *aoa);
void aoa_thread_stop(struct aoa *aoa);
void aoa_thread_join(struct aoa *aoa);
void aoa_destroy(struct aoa *aoa);

#endif
