#include "mouse_aoa.h"

#include <assert.h>

#include "hid/hid_mouse.h"
#include "input_events.h"
#include "util/log.h"

/** Downcast mouse processor to mouse_aoa */
#define DOWNCAST(MP) container_of(MP, struct sc_mouse_aoa, mouse_processor)

#define HID_MOUSE_ACCESSORY_ID 2

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_motion_event *event) {
    struct sc_mouse_aoa *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    sc_hid_mouse_event_from_motion(&hid_event, event);

    if (!sc_aoa_push_hid_event(mouse->aoa, HID_MOUSE_ACCESSORY_ID,
                               &hid_event)) {
        LOGW("Could not request HID event (mouse motion)");
    }
}

static void
sc_mouse_processor_process_mouse_click(struct sc_mouse_processor *mp,
                                   const struct sc_mouse_click_event *event) {
    struct sc_mouse_aoa *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    sc_hid_mouse_event_from_click(&hid_event, event);

    if (!sc_aoa_push_hid_event(mouse->aoa, HID_MOUSE_ACCESSORY_ID,
                               &hid_event)) {
        LOGW("Could not request HID event (mouse click)");
    }
}

static void
sc_mouse_processor_process_mouse_scroll(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_scroll_event *event) {
    struct sc_mouse_aoa *mouse = DOWNCAST(mp);

    struct sc_hid_event hid_event;
    sc_hid_mouse_event_from_scroll(&hid_event, event);

    if (!sc_aoa_push_hid_event(mouse->aoa, HID_MOUSE_ACCESSORY_ID,
                               &hid_event)) {
        LOGW("Could not request HID event (mouse scroll)");
    }
}

bool
sc_mouse_aoa_init(struct sc_mouse_aoa *mouse, struct sc_aoa *aoa) {
    mouse->aoa = aoa;

    bool ok = sc_aoa_setup_hid(aoa, HID_MOUSE_ACCESSORY_ID,
                               SC_HID_MOUSE_REPORT_DESC,
                               SC_HID_MOUSE_REPORT_DESC_LEN);
    if (!ok) {
        LOGW("Register HID mouse failed");
        return false;
    }

    static const struct sc_mouse_processor_ops ops = {
        .process_mouse_motion = sc_mouse_processor_process_mouse_motion,
        .process_mouse_click = sc_mouse_processor_process_mouse_click,
        .process_mouse_scroll = sc_mouse_processor_process_mouse_scroll,
        // Touch events not supported (coordinates are not relative)
        .process_touch = NULL,
    };

    mouse->mouse_processor.ops = &ops;

    mouse->mouse_processor.relative_mode = true;

    return true;
}

void
sc_mouse_aoa_destroy(struct sc_mouse_aoa *mouse) {
    bool ok = sc_aoa_unregister_hid(mouse->aoa, HID_MOUSE_ACCESSORY_ID);
    if (!ok) {
        LOGW("Could not unregister HID mouse");
    }
}
