#ifndef SC_HID_MOUSE_H
#define SC_HID_MOUSE_H

#include "common.h"

#include "hid/hid_event.h"
#include "input_events.h"

#define SC_HID_ID_MOUSE 2

struct sc_hid_mouse {
    float residual_hscroll;
    float residual_vscroll;
};

void sc_hid_mouse_init(struct sc_hid_mouse *hid);

void
sc_hid_mouse_generate_open(struct sc_hid_open *hid_open);

void
sc_hid_mouse_generate_close(struct sc_hid_close *hid_close);

void
sc_hid_mouse_generate_input_from_motion(struct sc_hid_input *hid_input,
                                    const struct sc_mouse_motion_event *event);

void
sc_hid_mouse_generate_input_from_click(struct sc_hid_input *hid_input,
                                    const struct sc_mouse_click_event *event);

bool
sc_hid_mouse_generate_input_from_scroll(struct sc_hid_mouse *hid,
                                        struct sc_hid_input *hid_input,
                                    const struct sc_mouse_scroll_event *event);

#endif
