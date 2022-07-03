#include "common.h"

#include <assert.h>
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

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 14);

    const unsigned char expected[] = {
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

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 18);

    const unsigned char expected[] = {
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

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 5 + SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH);

    unsigned char expected[5 + SC_CONTROL_MSG_INJECT_TEXT_MAX_LENGTH];
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
            .buttons = AMOTION_EVENT_BUTTON_PRIMARY,
        },
    };

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 28);

    const unsigned char expected[] = {
        SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT,
        0x00, // AKEY_EVENT_ACTION_DOWN
        0x12, 0x34, 0x56, 0x78, 0x87, 0x65, 0x43, 0x21, // pointer id
        0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0xc8, // 100 200
        0x04, 0x38, 0x07, 0x80, // 1080 1920
        0xff, 0xff, // pressure
        0x00, 0x00, 0x00, 0x01 // AMOTION_EVENT_BUTTON_PRIMARY
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
            .hscroll = 1,
            .vscroll = -1,
            .buttons = 1,
        },
    };

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 21);

    const unsigned char expected[] = {
        SC_CONTROL_MSG_TYPE_INJECT_SCROLL_EVENT,
        0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x04, 0x02, // 260 1026
        0x04, 0x38, 0x07, 0x80, // 1080 1920
        0x7F, 0xFF, // 1 (float encoded as i16)
        0x80, 0x00, // -1 (float encoded as i16)
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

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 2);

    const unsigned char expected[] = {
        SC_CONTROL_MSG_TYPE_BACK_OR_SCREEN_ON,
        0x01, // AKEY_EVENT_ACTION_UP
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_expand_notification_panel(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    };

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const unsigned char expected[] = {
        SC_CONTROL_MSG_TYPE_EXPAND_NOTIFICATION_PANEL,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_expand_settings_panel(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL,
    };

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const unsigned char expected[] = {
        SC_CONTROL_MSG_TYPE_EXPAND_SETTINGS_PANEL,
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_collapse_panels(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_COLLAPSE_PANELS,
    };

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const unsigned char expected[] = {
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

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 2);

    const unsigned char expected[] = {
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

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 27);

    const unsigned char expected[] = {
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

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == SC_CONTROL_MSG_MAX_SIZE);

    unsigned char expected[SC_CONTROL_MSG_MAX_SIZE] = {
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

static void test_serialize_set_screen_power_mode(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE,
        .set_screen_power_mode = {
            .mode = SC_SCREEN_POWER_MODE_NORMAL,
        },
    };

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 2);

    const unsigned char expected[] = {
        SC_CONTROL_MSG_TYPE_SET_SCREEN_POWER_MODE,
        0x02, // SC_SCREEN_POWER_MODE_NORMAL
    };
    assert(!memcmp(buf, expected, sizeof(expected)));
}

static void test_serialize_rotate_device(void) {
    struct sc_control_msg msg = {
        .type = SC_CONTROL_MSG_TYPE_ROTATE_DEVICE,
    };

    unsigned char buf[SC_CONTROL_MSG_MAX_SIZE];
    size_t size = sc_control_msg_serialize(&msg, buf);
    assert(size == 1);

    const unsigned char expected[] = {
        SC_CONTROL_MSG_TYPE_ROTATE_DEVICE,
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
    test_serialize_set_screen_power_mode();
    test_serialize_rotate_device();
    return 0;
}
