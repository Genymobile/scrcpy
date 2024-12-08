#include "mouse_uhid.h"

#include "hid/hid_mouse.h"
#include "input_events.h"
#include "util/log.h"

/** Downcast mouse processor to mouse_uhid */
#define DOWNCAST(MP) container_of(MP, struct sc_mouse_uhid, mouse_processor)

static void
sc_mouse_uhid_send_input(struct sc_mouse_uhid *mouse,
                         const struct sc_hid_input *hid_input,
                         const char *name) {
    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_UHID_INPUT;
    msg.uhid_input.id = hid_input->hid_id;

    assert(hid_input->size <= SC_HID_MAX_SIZE);
    memcpy(msg.uhid_input.data, hid_input->data, hid_input->size);
    msg.uhid_input.size = hid_input->size;

    if (!sc_controller_push_msg(mouse->controller, &msg)) {
        LOGE("Could not push UHID_INPUT message (%s)", name);
    }
}

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_motion_event *event) {
    struct sc_mouse_uhid *mouse = DOWNCAST(mp);

    struct sc_hid_input hid_input;
    sc_hid_mouse_generate_input_from_motion(&hid_input, event);

    sc_mouse_uhid_send_input(mouse, &hid_input, "mouse motion");
}

static void
sc_mouse_processor_process_mouse_click(struct sc_mouse_processor *mp,
                                   const struct sc_mouse_click_event *event) {
    struct sc_mouse_uhid *mouse = DOWNCAST(mp);

    struct sc_hid_input hid_input;
    sc_hid_mouse_generate_input_from_click(&hid_input, event);

    sc_mouse_uhid_send_input(mouse, &hid_input, "mouse click");
}

static void
sc_mouse_processor_process_mouse_scroll(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_scroll_event *event) {
    struct sc_mouse_uhid *mouse = DOWNCAST(mp);

    struct sc_hid_input hid_input;
    sc_hid_mouse_generate_input_from_scroll(&hid_input, event);

    sc_mouse_uhid_send_input(mouse, &hid_input, "mouse scroll");
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

    struct sc_hid_open hid_open;
    sc_hid_mouse_generate_open(&hid_open);
    assert(hid_open.hid_id == SC_HID_ID_MOUSE);

    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_UHID_CREATE;
    msg.uhid_create.id = SC_HID_ID_MOUSE;
    msg.uhid_create.vendor_id = 0;
    msg.uhid_create.product_id = 0;
    msg.uhid_create.name = NULL;
    msg.uhid_create.report_desc = hid_open.report_desc;
    msg.uhid_create.report_desc_size = hid_open.report_desc_size;
    if (!sc_controller_push_msg(controller, &msg)) {
        LOGE("Could not push UHID_CREATE message (mouse)");
        return false;
    }

    return true;
}
