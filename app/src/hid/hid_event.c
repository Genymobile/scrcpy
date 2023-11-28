#include <stdlib.h>

#include "hid_event.h"
#include "util/acksync.h"

void
sc_hid_event_init(struct sc_hid_event *hid_event, uint16_t accessory_id,
                  uint8_t *buffer, uint16_t buffer_size) {
    hid_event->accessory_id = accessory_id;
    hid_event->buffer = buffer;
    hid_event->size = buffer_size;
    hid_event->ack_to_wait = SC_SEQUENCE_INVALID;
}

void
sc_hid_event_destroy(struct sc_hid_event *hid_event) {
    free(hid_event->buffer);
}
