#include "hid_mouse.h"

#include <stdint.h>

// 1 byte for buttons + padding, 1 byte for X position, 1 byte for Y position,
// 1 byte for wheel motion, 1 byte for hozizontal scrolling
#define SC_HID_MOUSE_INPUT_SIZE 5

/**
 * Mouse descriptor from the specification:
 * <https://www.usb.org/document-library/device-class-definition-hid-111>
 *
 * Appendix E (p71): §E.10 Report Descriptor (Mouse)
 *
 * The usage tags (like Wheel) are listed in "HID Usage Tables":
 * <https://www.usb.org/document-library/hid-usage-tables-15>
 * §4 Generic Desktop Page (0x01) (p32)
 */
static const uint8_t SC_HID_MOUSE_REPORT_DESC[] = {
    // Usage Page (Generic Desktop)
    0x05, 0x01,
    // Usage (Mouse)
    0x09, 0x02,

    // Collection (Application)
    0xA1, 0x01,

    // Usage (Pointer)
    0x09, 0x01,

    // Collection (Physical)
    0xA1, 0x00,

    // Usage Page (Buttons)
    0x05, 0x09,

    // Usage Minimum (1)
    0x19, 0x01,
    // Usage Maximum (5)
    0x29, 0x05,
    // Logical Minimum (0)
    0x15, 0x00,
    // Logical Maximum (1)
    0x25, 0x01,
    // Report Count (5)
    0x95, 0x05,
    // Report Size (1)
    0x75, 0x01,
    // Input (Data, Variable, Absolute): 5 buttons bits
    0x81, 0x02,

    // Report Count (1)
    0x95, 0x01,
    // Report Size (3)
    0x75, 0x03,
    // Input (Constant): 3 bits padding
    0x81, 0x01,

    // Usage Page (Generic Desktop)
    0x05, 0x01,
    // Usage (X)
    0x09, 0x30,
    // Usage (Y)
    0x09, 0x31,
    // Usage (Wheel)
    0x09, 0x38,
    // Logical Minimum (-127)
    0x15, 0x81,
    // Logical Maximum (127)
    0x25, 0x7F,
    // Report Size (8)
    0x75, 0x08,
    // Report Count (3)
    0x95, 0x03,
    // Input (Data, Variable, Relative): 3 position bytes (X, Y, Wheel)
    0x81, 0x06,

    // Usage Page (Consumer Page)
    0x05, 0x0C,
    // Usage(AC Pan)
    0x0A, 0x38, 0x02,
    // Logical Minimum (-127)
    0x15, 0x81,
    // Logical Maximum (127)
    0x25, 0x7F,
    // Report Size (8)
    0x75, 0x08,
    // Report Count (1)
    0x95, 0x01,
    // Input (Data, Variable, Relative): 1 byte (AC Pan)
    0x81, 0x06,

    // End Collection
    0xC0,

    // End Collection
    0xC0,
};

/**
 * A mouse HID input report is 4 bytes long:
 *
 *  - byte 0: buttons state
 *  - byte 1: relative x motion (signed byte from -127 to 127)
 *  - byte 2: relative y motion (signed byte from -127 to 127)
 *  - byte 3: wheel motion (-1, 0 or 1)
 *
 *                   7 6 5 4 3 2 1 0
 *                  +---------------+
 *         byte 0:  |0 0 0 . . . . .| buttons state
 *                  +---------------+
 *                         ^ ^ ^ ^ ^
 *                         | | | | `- left button
 *                         | | | `--- right button
 *                         | | `----- middle button
 *                         | `------- button 4
 *                         `--------- button 5
 *
 *                  +---------------+
 *         byte 1:  |. . . . . . . .| relative x motion
 *                  +---------------+
 *         byte 2:  |. . . . . . . .| relative y motion
 *                  +---------------+
 *         byte 3:  |. . . . . . . .| wheel motion
 *                  +---------------+
 *
 * As an example, here is the report for a motion of (x=5, y=-4) with left
 * button pressed:
 *
 *                  +---------------+
 *                  |0 0 0 0 0 0 0 1| left button pressed
 *                  +---------------+
 *                  |0 0 0 0 0 1 0 1| horizontal motion (x = 5)
 *                  +---------------+
 *                  |1 1 1 1 1 1 0 0| relative y motion (y = -4)
 *                  +---------------+
 *                  |0 0 0 0 0 0 0 0| wheel motion
 *                  +---------------+
 */

static void
sc_hid_mouse_input_init(struct sc_hid_input *hid_input) {
    hid_input->hid_id = SC_HID_ID_MOUSE;
    hid_input->size = SC_HID_MOUSE_INPUT_SIZE;
    // Leave ->data uninitialized, it will be fully initialized by callers
}

static uint8_t
sc_hid_buttons_from_buttons_state(uint8_t buttons_state) {
    uint8_t c = 0;
    if (buttons_state & SC_MOUSE_BUTTON_LEFT) {
        c |= 1 << 0;
    }
    if (buttons_state & SC_MOUSE_BUTTON_RIGHT) {
        c |= 1 << 1;
    }
    if (buttons_state & SC_MOUSE_BUTTON_MIDDLE) {
        c |= 1 << 2;
    }
    if (buttons_state & SC_MOUSE_BUTTON_X1) {
        c |= 1 << 3;
    }
    if (buttons_state & SC_MOUSE_BUTTON_X2) {
        c |= 1 << 4;
    }
    return c;
}

void
sc_hid_mouse_generate_input_from_motion(struct sc_hid_input *hid_input,
                                    const struct sc_mouse_motion_event *event) {
    sc_hid_mouse_input_init(hid_input);

    uint8_t *data = hid_input->data;
    data[0] = sc_hid_buttons_from_buttons_state(event->buttons_state);
    data[1] = CLAMP(event->xrel, -127, 127);
    data[2] = CLAMP(event->yrel, -127, 127);
    data[3] = 0; // no vertical scrolling
    data[4] = 0; // no horizontal scrolling
}

void
sc_hid_mouse_generate_input_from_click(struct sc_hid_input *hid_input,
                                     const struct sc_mouse_click_event *event) {
    sc_hid_mouse_input_init(hid_input);

    uint8_t *data = hid_input->data;
    data[0] = sc_hid_buttons_from_buttons_state(event->buttons_state);
    data[1] = 0; // no x motion
    data[2] = 0; // no y motion
    data[3] = 0; // no vertical scrolling
    data[4] = 0; // no horizontal scrolling
}

bool
sc_hid_mouse_generate_input_from_scroll(struct sc_hid_input *hid_input,
                                    const struct sc_mouse_scroll_event *event) {
    if (!event->vscroll_int && !event->hscroll_int) {
        // Need a full integral value for HID
        return false;
    }

    sc_hid_mouse_input_init(hid_input);

    uint8_t *data = hid_input->data;
    data[0] = 0; // buttons state irrelevant (and unknown)
    data[1] = 0; // no x motion
    data[2] = 0; // no y motion
    data[3] = CLAMP(event->vscroll_int, -127, 127);
    data[4] = CLAMP(event->hscroll_int, -127, 127);
    return true;
}

void sc_hid_mouse_generate_open(struct sc_hid_open *hid_open) {
    hid_open->hid_id = SC_HID_ID_MOUSE;
    hid_open->report_desc = SC_HID_MOUSE_REPORT_DESC;
    hid_open->report_desc_size = sizeof(SC_HID_MOUSE_REPORT_DESC);
}

void sc_hid_mouse_generate_close(struct sc_hid_close *hid_close) {
    hid_close->hid_id = SC_HID_ID_MOUSE;
}
