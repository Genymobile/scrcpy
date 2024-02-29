#ifndef SC_HID_EVENT_H
#define SC_HID_EVENT_H

#include "common.h"

#include <stdint.h>
#include "util/tick.h"

struct sc_hid_event {
    uint16_t accessory_id;
    unsigned char *buffer;
    uint16_t size;
    uint64_t ack_to_wait;
    sc_tick timestamp; // Only used by hid_replay.c & hid_event_serializer.c
};

// Takes ownership of buffer
void
sc_hid_event_init(struct sc_hid_event *hid_event, uint16_t accessory_id,
                  unsigned char *buffer, uint16_t buffer_size);

void
sc_hid_event_destroy(struct sc_hid_event *hid_event);
#endif
