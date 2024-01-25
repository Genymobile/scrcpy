#ifndef SC_HID_MOUSE_H
#define SC_HID_MOUSE_H

#endif

#include "common.h"

#include <stdbool.h>

#include "hid/hid_event.h"
#include "input_events.h"

extern const uint8_t SC_HID_MOUSE_REPORT_DESC[];
extern const size_t SC_HID_MOUSE_REPORT_DESC_LEN;

void
sc_hid_mouse_event_from_motion(struct sc_hid_event *hid_event,
                               const struct sc_mouse_motion_event *event);

void
sc_hid_mouse_event_from_click(struct sc_hid_event *hid_event,
                              const struct sc_mouse_click_event *event);

void
sc_hid_mouse_event_from_scroll(struct sc_hid_event *hid_event,
                               const struct sc_mouse_scroll_event *event);
