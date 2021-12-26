#include "hid_mouse.h"

#include <assert.h>

#include "input_events.h"
#include "util/log.h"

/** Downcast mouse processor to hid_mouse */
#define DOWNCAST(MP) container_of(MP, struct sc_hid_mouse, mouse_processor)

#define HID_MOUSE_ACCESSORY_ID 2

// 1 byte for buttons + padding, 1 byte for X position, 1 byte for Y position
#define HID_MOUSE_EVENT_SIZE 3

/**
 * Mouse descriptor from the specification:
 * <https://www.usb.org/sites/default/files/hid1_11.pdf>
 *
 * Appendix E (p71): §E.10 Report Descriptor (Mouse)
 */
static const unsigned char mouse_report_desc[]  = {
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
     // Usage Maximum (3)
    0x29, 0x03,
    // Logical Minimum (0)
    0x15, 0x00,
    // Logical Maximum (1)
    0x25, 0x01,
    // Report Count (3)
    0x95, 0x03,
    // Report Size (1)
    0x75, 0x01,
    // Input (Data, Variable, Absolute): 3 buttons bits
    0x81, 0x02,

    // Report Count (1)
    0x95, 0x01,
    // Report Size (5)
    0x75, 0x05,
    // Input (Constant): 5 bits padding
    0x81, 0x01,

    // Usage Page (Generic Desktop)
    0x05, 0x01,
    // Usage (X)
    0x09, 0x30,
    // Usage (Y)
    0x09, 0x31,
    // Local Minimum (-127)
    0x15, 0x81,
    // Local Maximum (127)
    0x25, 0x7F,
    // Report Size (8)
    0x75, 0x08,
    // Report Count (2)
    0x95, 0x02,
    // Input (Data, Variable, Relative): 2 position bytes (X & Y)
    0x81, 0x06,

    // End Collection
    0xC0,

    // End Collection
    0xC0,
};

/**
 * A mouse HID event is 3 bytes long:
 *
 *  - byte 0: buttons state
 *  - byte 1: relative x motion (signed byte from -127 to 127)
 *  - byte 2: relative y motion (signed byte from -127 to 127)
 *
 *                   7 6 5 4 3 2 1 0
 *                  +---------------+
 *         byte 0:  |0 0 0 0 0 . . .| buttons state
 *                  +---------------+
 *                             ^ ^ ^
 *                             | | `- left button
 *                             | `--- right button
 *                             `----- middle button
 *
 *                  +---------------+
 *         byte 1:  |. . . . . . . .| relative x motion
 *                  +---------------+
 *         byte 2:  |. . . . . . . .| relative y motion
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
 */

static bool
sc_hid_mouse_event_init(struct sc_hid_event *hid_event) {
    unsigned char *buffer = calloc(1, HID_MOUSE_EVENT_SIZE);
    if (!buffer) {
        LOG_OOM();
        return false;
    }

    sc_hid_event_init(hid_event, HID_MOUSE_ACCESSORY_ID, buffer,
                      HID_MOUSE_EVENT_SIZE);
    return true;
}

static unsigned char
buttons_state_to_hid_buttons(uint8_t buttons_state) {
    unsigned char c = 0;
    if (buttons_state & SC_MOUSE_BUTTON_LEFT) {
        c |= 1 << 0;
    }
    if (buttons_state & SC_MOUSE_BUTTON_RIGHT) {
        c |= 1 << 1;
    }
    if (buttons_state & SC_MOUSE_BUTTON_MIDDLE) {
        c |= 1 << 2;
    }
    // TODO buttons 4 and 5?
    return c;
}

static bool
convert_hid_mouse_motion(struct sc_hid_mouse *mouse,
                         struct sc_hid_event *hid_event,
                         const struct sc_mouse_motion_event *event) {
    if (!sc_hid_mouse_event_init(hid_event)) {
        LOGW("Could not initialize HID mouse event");
        return false;
    }

    unsigned char *buffer = hid_event->buffer;
    buffer[0] = buttons_state_to_hid_buttons(event->buttons_state);
    buffer[1] = CLAMP(event->xrel, -127, 127);
    buffer[2] = CLAMP(event->yrel, -127, 127);

    return true;
}

static bool
convert_hid_mouse_click(struct sc_hid_mouse *mouse,
                        struct sc_hid_event *hid_event,
                        const struct sc_mouse_click_event *event) {
    if (!sc_hid_mouse_event_init(hid_event)) {
        LOGW("Could not initialize HID mouse event");
        return false;
    }

    unsigned char *buffer = hid_event->buffer;
    buffer[0] = buttons_state_to_hid_buttons(event->buttons_state);
    buffer[1] = 0; // no x motion
    buffer[2] = 0; // no y motion

    return true;
}

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_motion_event *event) {
    struct sc_hid_mouse *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    if (!convert_hid_mouse_motion(mouse, &hid_event, event)) {
        return;
    }

    if (!sc_aoa_push_hid_event(mouse->aoa, &hid_event)) {
        sc_hid_event_destroy(&hid_event);
        LOGW("Could request HID event");
    }
}

static void
sc_mouse_processor_process_mouse_click(struct sc_mouse_processor *mp,
                                   const struct sc_mouse_click_event *event) {
    LOGD("button");
    struct sc_hid_mouse *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    if (!convert_hid_mouse_click(mouse, &hid_event, event)) {
        return;
    }

    if (!sc_aoa_push_hid_event(mouse->aoa, &hid_event)) {
        sc_hid_event_destroy(&hid_event);
        LOGW("Could request HID event");
    }
}

static void
sc_mouse_processor_process_mouse_scroll(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_scroll_event *event) {
    struct sc_hid_mouse *mouse = DOWNCAST(mp);
}

static void
sc_mouse_processor_process_touch(struct sc_mouse_processor *mp,
                                 const struct sc_touch_event *event) {
    struct sc_hid_mouse *mouse = DOWNCAST(mp);
}

bool
sc_hid_mouse_init(struct sc_hid_mouse *mouse, struct sc_aoa *aoa) {
    mouse->aoa = aoa;

    bool ok = sc_aoa_setup_hid(aoa, HID_MOUSE_ACCESSORY_ID, mouse_report_desc,
                               ARRAY_LEN(mouse_report_desc));
    if (!ok) {
        LOGW("Register HID mouse failed");
        return false;
    }

    static const struct sc_mouse_processor_ops ops = {
        .process_mouse_motion = sc_mouse_processor_process_mouse_motion,
        .process_mouse_click = sc_mouse_processor_process_mouse_click,
        .process_mouse_scroll = sc_mouse_processor_process_mouse_scroll,
        .process_touch = sc_mouse_processor_process_touch,
    };

    mouse->mouse_processor.ops = &ops;

    return true;
}

void
sc_hid_mouse_destroy(struct sc_hid_mouse *mouse) {
    bool ok = sc_aoa_unregister_hid(mouse->aoa, HID_MOUSE_ACCESSORY_ID);
    if (!ok) {
        LOGW("Could not unregister HID");
    }
}
