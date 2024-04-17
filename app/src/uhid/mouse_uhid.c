#include "mouse_uhid.h"

#include "hid/hid_mouse.h"
#include "input_events.h"
#include "util/log.h"

/** Downcast mouse processor to mouse_uhid */
#define DOWNCAST(MP) container_of(MP, struct sc_mouse_uhid, mouse_processor)

#define UHID_MOUSE_ID 2

static void
sc_mouse_uhid_send_input(struct sc_mouse_uhid *mouse,
                         const struct sc_hid_event *event, const char *name) {
    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_UHID_INPUT;
    msg.uhid_input.id = UHID_MOUSE_ID;

    assert(event->size <= SC_HID_MAX_SIZE);
    memcpy(msg.uhid_input.data, event->data, event->size);
    msg.uhid_input.size = event->size;

    if (!sc_controller_push_msg(mouse->controller, &msg)) {
        LOGE("Could not send UHID_INPUT message (%s)", name);
    }
}

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_motion_event *event) {
    struct sc_mouse_uhid *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    sc_hid_mouse_event_from_motion(&hid_event, event);

    sc_mouse_uhid_send_input(mouse, &hid_event, "mouse motion");
}

static void
sc_mouse_processor_process_mouse_click(struct sc_mouse_processor *mp,
                                   const struct sc_mouse_click_event *event) {
    struct sc_mouse_uhid *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    sc_hid_mouse_event_from_click(&hid_event, event);

    sc_mouse_uhid_send_input(mouse, &hid_event, "mouse click");
}

static void
sc_mouse_processor_process_mouse_scroll(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_scroll_event *event) {
    struct sc_mouse_uhid *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    sc_hid_mouse_event_from_scroll(&hid_event, event);

    sc_mouse_uhid_send_input(mouse, &hid_event, "mouse scroll");
}

bool
sc_mouse_uhid_init(struct sc_mouse_uhid *mouse,
                   struct sc_controller *controller) {
    mouse->controller = controller;

    static const struct sc_mouse_processor_ops ops = {
        .process_mouse_motion = sc_mouse_processor_process_mouse_motion,
        .process_mouse_click = sc_mouse_processor_process_mouse_click,
        .process_mouse_scroll = sc_mouse_processor_process_mouse_scroll,
        // Touch events not supported (coordinates are not relative)
        .process_touch = NULL,
    };

    mouse->mouse_processor.ops = &ops;

    mouse->mouse_processor.relative_mode = true;

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_UHID_CREATE;
    msg.uhid_create.id = UHID_MOUSE_ID;
    msg.uhid_create.report_desc = SC_HID_MOUSE_REPORT_DESC;
    msg.uhid_create.report_desc_size = SC_HID_MOUSE_REPORT_DESC_LEN;
    if (!sc_controller_push_msg(controller, &msg)) {
        LOGE("Could not send UHID_CREATE message (mouse)");
        return false;
    }

    return true;
}
