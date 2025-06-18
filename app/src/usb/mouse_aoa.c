#include "mouse_aoa.h"

#include <assert.h>
#include <stddef.h>

#include "hid/hid_mouse.h"
#include "input_events.h"
#include "util/log.h"

/** Downcast mouse processor to mouse_aoa */
#define DOWNCAST(MP) container_of(MP, struct sc_mouse_aoa, mouse_processor)

static void
sc_mouse_processor_process_mouse_motion(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_motion_event *event) {
    struct sc_mouse_aoa *mouse = DOWNCAST(mp);

    struct sc_hid_input hid_input;
    sc_hid_mouse_generate_input_from_motion(&hid_input, event);

    if (!sc_aoa_push_input(mouse->aoa, &hid_input)) {
        LOGW("Could not push AOA HID input (mouse motion)");
    }
}

static void
sc_mouse_processor_process_mouse_click(struct sc_mouse_processor *mp,
                                   const struct sc_mouse_click_event *event) {
    struct sc_mouse_aoa *mouse = DOWNCAST(mp);

    struct sc_hid_input hid_input;
    sc_hid_mouse_generate_input_from_click(&hid_input, event);

    if (!sc_aoa_push_input(mouse->aoa, &hid_input)) {
        LOGW("Could not push AOA HID input (mouse click)");
    }
}

static void
sc_mouse_processor_process_mouse_scroll(struct sc_mouse_processor *mp,
                                    const struct sc_mouse_scroll_event *event) {
    struct sc_mouse_aoa *mouse = DOWNCAST(mp);

    struct sc_hid_input hid_input;
    if (!sc_hid_mouse_generate_input_from_scroll(&hid_input, event)) {
        return;
    }

    if (!sc_aoa_push_input(mouse->aoa, &hid_input)) {
        LOGW("Could not push AOA HID input (mouse scroll)");
    }
}

bool
sc_mouse_aoa_init(struct sc_mouse_aoa *mouse, struct sc_aoa *aoa) {
    mouse->aoa = aoa;

    struct sc_hid_open hid_open;
    sc_hid_mouse_generate_open(&hid_open);

    bool ok = sc_aoa_push_open(aoa, &hid_open, true);
    if (!ok) {
        LOGW("Could not push AOA HID open (mouse)");
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
    (void) mouse;
    // Do nothing, mouse->aoa will automatically unregister all devices
}
