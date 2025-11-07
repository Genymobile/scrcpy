#include "common.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "control_msg.h"

static void test_serialize_inject_keycode(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_KEYCODE,
        .inject_keycode = {
            .action = AKEY_EVENT_ACTION_UP,
            .keycode = AKEYCODE_ENTER,
            .repeat = 5,
            .metastate = AMETA_SHIFT_ON | AMETA_SHIFT_LEFT_ON,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 14);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_INJECT_KEYCODE,
        0x01, // AKEY_EVENT_ACTION_UP
        0x00, 0x00, 0x00, 0x42, // AKEYCODE_ENTER
        0x00, 0x00, 0x00, 0X05, // repeat
        0x00, 0x00, 0x00, 0x41, // AMETA_SHIFT_ON | AMETA_SHIFT_LEFT_ON
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_inject_text(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_TEXT,
        .inject_text = {
            .text = "hello, world!",
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 18);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_INJECT_TEXT,
        0x00, 0x00, 0x00, 0x0d, // text length
        'h', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!', // text
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_inject_text_long(void) {
    struct sc_control_msg msg;
    msg.type = SC_CONTROL_MSG_TYPE_INJECT_TEXT;
    char text[SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH + 1];
    memset(text, 'a', SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH);
    text[SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH] = '\0';
    msg.inject_text.text = text;

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 5 + SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH);

    uint8_t expected[5 + SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH];
    expected[0] = SC_CONTROL_MSG_TYPE_INJECT_TEXT;
    expected[1] = 0x00;
    expected[2] = 0x00;
    expected[3] = 0x01;
    expected[4] = 0x2c; // text length (32 bits)
    memset(&expected[5], 'a', SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH);

    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_inject_touch_event(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
        .inject_touch_event = {
            .action = AMOTION_EVENT_ACTION_DOWN,
            .pointer_id = UINT64_C(0x1234567887654321),
            .position = {
                .point = {
                    .x = 100,
                    .y = 200,
                },
                .screen_size = {
                    .width = 1080,
                    .height = 1920,
                },
            },
            .pressure = 1.0f,
            .action_button = AMOTION_EVENT_BUTTON_PRIMARY,
            .buttons = AMOTION_EVENT_BUTTON_PRIMARY,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 32);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
        0x00, // AKEY_EVENT_ACTION_DOWN
        0x12, 0x34, 0x56, 0x78, 0x87, 0x65, 0x43, 0x21, // pointer id
        0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0xc8, // 100 200
        0x04, 0x38, 0x07, 0x80, // 1080 1920
        0xff, 0xff, // pressure
        0x00, 0x00, 0x00, 0x01, // AMOTION_EVENT_BUTTON_PRIMARY (action button)
        0x00, 0x00, 0x00, 0x01, // AMOTION_EVENT_BUTTON_PRIMARY (buttons)
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_inject_scroll_event(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
        .inject_scroll_event = {
            .position = {
                .point = {
                    .x = 260,
                    .y = 1026,
                },
                .screen_size = {
                    .width = 1080,
                    .height = 1920,
                },
            },
            .hscroll = 16,
            .vscroll = -16,
            .buttons = 1,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 21);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
        0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x04, 0x02, // 260 1026
        0x04, 0x38, 0x07, 0x80, // 1080 1920
        0x7F, 0xFF, // 16 (float encoded as i16 in the range [-16, 16])
        0x80, 0x00, // -16 (float encoded as i16 in the range [-16, 16])
        0x00, 0x00, 0x00, 0x01, // 1
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_back_or_screen_on(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON,
        .back_or_screen_on = {
            .action = AKEY_EVENT_ACTION_UP,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 2);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON,
        0x01, // AKEY_EVENT_ACTION_UP
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_expand_notification_panel(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_expand_settings_panel(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL,
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_collapse_panels(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS,
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_get_clipboard(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_GET_CLIPBOARD,
        .get_clipboard = {
            .copy_key = SC_COPY_KEY_COPY,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 2);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_GET_CLIPBOARD,
        SC_COPY_KEY_COPY,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_set_clipboard(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_SET_CLIPBOARD,
        .set_clipboard = {
            .sequence = UINT64_C(0x0102030405060708),
            .paste = true,
            .text = "hello, world!",
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 27);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_SET_CLIPBOARD,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // sequence
        1, // paste
        0x00, 0x00, 0x00, 0x0d, // text length
        'h', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!', // text
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_set_clipboard_long(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_SET_CLIPBOARD,
        .set_clipboard = {
            .sequence = UINT64_C(0x0102030405060708),
            .paste = true,
            .text = NULL,
        },
    };

    char text[SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH + 1];
    memset(text, 'a', SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH);
    text[SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH] = '\0';
    msg.set_clipboard.text = text;

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == SC_CONTROL_MSG_MAX_SIZE);

    uint8_t expected[SC_CONTROL_MSG_MAX_SIZE] = {
        SC_CONTROL_MSG_TYPE_SET_CLIPBOARD,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // sequence
        1, // paste
        // text length
        SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH >> 24,
        (SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH >> 16) & 0xff,
        (SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH >> 8) & 0xff,
        SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH & 0xff,
    };
    memset(expected + 14, 'a', SC_CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH);

    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_set_display_power(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_SET_DISPLAY_POWER,
        .set_display_power = {
            .on = true,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 2);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_SET_DISPLAY_POWER,
        0x01, // true
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_rotate_device(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_ROTATE_DEVICE,
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_ROTATE_DEVICE,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_uhid_create(void) {
    const uint8_t report_desc[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_UHID_CREATE,
        .uhid_create = {
            .id = 42,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .name = "ABC",
            .report_desc_size = sizeof(report_desc),
            .report_desc = report_desc,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 24);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_UHID_CREATE,
        0, 42, // id
        0x12, 0x34, // vendor id
        0x56, 0x78, // product id
        3, // name size
        65, 66, 67, // "ABC"
        0, 11, // report desc size
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_uhid_input(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_UHID_INPUT,
        .uhid_input = {
            .id = 42,
            .size = 5,
            .data = {1, 2, 3, 4, 5},
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 10);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_UHID_INPUT,
        0, 42, // id
        0, 5, // size
        1, 2, 3, 4, 5,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_uhid_destroy(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_UHID_DESTROY,
        .uhid_destroy = {
            .id = 42,
        },
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 3);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_UHID_DESTROY,
        0, 42, // id
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_open_hard_keyboard(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_OPEN_HARD_KEYBOARD_SETTINGS,
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_OPEN_HARD_KEYBOARD_SETTINGS,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_reset_video(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_RESET_VIDEO,
    };

    uint8_t buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const uint8_t expected[] = {
        SC_CONTROL_MSG_TYPE_RESET_VIDEO,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_serialize_inject_keycode();
    test_serialize_inject_text();
    test_serialize_inject_text_long();
    test_serialize_inject_touch_event();
    test_serialize_inject_scroll_event();
    test_serialize_back_or_screen_on();
    test_serialize_expand_notification_panel();
    test_serialize_expand_settings_panel();
    test_serialize_collapse_panels();
    test_serialize_get_clipboard();
    test_serialize_set_clipboard();
    test_serialize_set_clipboard_long();
    test_serialize_set_display_power();
    test_serialize_rotate_device();
    test_serialize_uhid_create();
    test_serialize_uhid_input();
    test_serialize_uhid_destroy();
    test_serialize_open_hard_keyboard();
    test_serialize_reset_video();
    return 0;
}
