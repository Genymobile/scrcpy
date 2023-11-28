#include <stdbool.h>
#include <stdio.h>

#include "uhid.h"
#include "uhid_hid.h"
#include "hid_event.h"
#include "util/log.h"

/** Downcast hid interface to sc_uhid */
#define DOWNCAST(HI) container_of(HI, struct sc_uhid, hid_interface)

static bool
sc_uhid_create(struct sc_hid_interface *hid_interface, uint16_t id,
               uint16_t vendor_id, uint16_t product_id,
               const uint8_t* report_desc,
               uint16_t report_desc_size) {
    assert(report_desc_size <= HID_MAX_DESCRIPTOR_SIZE);

    struct sc_uhid *uhid = DOWNCAST(hid_interface);

    struct uhid_event *ev = malloc(sizeof(struct uhid_event));
    *ev = (struct uhid_event) {
        .type = UHID_CREATE2,
        .u.create2 = {
            .name = "scrcpy",
            .phys = "",
            .rd_size = report_desc_size,
            .bus = BUS_BLUETOOTH,
            .vendor = vendor_id,
            .product = product_id,
            .version = 0,
            .country = 0,
        },
    };

    if (!snprintf((char *)ev->u.create2.uniq, sizeof(ev->u.create2.uniq), "scrcpy-%d", id)) {
        LOGW("Could not set unique name");
        return false;
    }
    memcpy(ev->u.create2.rd_data, report_desc, report_desc_size);

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_UHID_OPEN,
        .uhid_open = {
            .id = id,
            .data = (uint8_t *) ev,
            .size = sizeof(*ev),
        },
    };

    if (!sc_controller_push_msg(uhid->controller, &msg)) {
        LOGW("Could not send UHID_OPEN message");
        return false;
    }

    return true;
}

static bool
sc_uhid_process_input(struct sc_hid_interface *hid_interface,
                      const struct sc_hid_event* event) {
    assert(event->size <= UHID_DATA_MAX);

    struct sc_uhid *uhid = DOWNCAST(hid_interface);

    struct uhid_event *ev = malloc(sizeof(struct uhid_event));
    *ev = (struct uhid_event) {
        .type = UHID_INPUT2,
        .u.input2 = {
            .size = event->size,
        },
    };
    memcpy(ev->u.input2.data, event->buffer, event->size);

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_UHID_WRITE,
        .uhid_write = {
            .id = event->accessory_id,
            .data = (uint8_t *) ev,
            .size = sizeof(*ev),
        },
    };

    if (!sc_controller_push_msg(uhid->controller, &msg)) {
        LOGW("Could not send UHID_WRITE message");
        return false;
    }

    return true;
}

static bool
sc_uhid_destroy(struct sc_hid_interface *hid_interface, uint16_t id) {
    struct sc_uhid *uhid = DOWNCAST(hid_interface);

    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_UHID_CLOSE,
        .uhid_close = {
            .id = id,
        },
    };

    if (!sc_controller_push_msg(uhid->controller, &msg)) {
        LOGW("Could not send UHID_CLOSE message");
        return false;
    }

    return true;
}

void sc_uhid_init(struct sc_uhid *uhid, struct sc_controller *controller) {
    static const struct sc_hid_interface_ops ops = {
        .create = sc_uhid_create,
        .process_input = sc_uhid_process_input,
        .destroy = sc_uhid_destroy,
    };

    uhid->hid_interface.async_message = false;
    uhid->hid_interface.ops = &ops;

    uhid->controller = controller;
}
