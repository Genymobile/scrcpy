#ifndef SC_HID_EVENT_H
#define SC_HID_EVENT_H

#include <stdint.h>

struct sc_hid_event {
    uint16_t accessory_id;
    uint8_t *buffer;
    uint16_t size;
    uint64_t ack_to_wait;
};

// Takes ownership of buffer
void
sc_hid_event_init(struct sc_hid_event *hid_event, uint16_t accessory_id,
                  uint8_t *buffer, uint16_t buffer_size);

void
sc_hid_event_destroy(struct sc_hid_event *hid_event);

#endif
