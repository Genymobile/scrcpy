#include <assert.h>
#include <string.h>

#include "device_msg.h"

#include <stdio.h>

static void test_deserialize_clipboard(void) {
    const unsigned char input[] = {
        DEVICE_MSG_TYPE_CLIPBOARD,
        0x00, 0x03, // text length
        0x41, 0x42, 0x43, // "ABC"
    };

    struct device_msg msg;
    ssize_t r = device_msg_deserialize(input, sizeof(input), &msg);
    assert(r == 6);

    assert(msg.type == DEVICE_MSG_TYPE_CLIPBOARD);
    assert(msg.clipboard.text);
    assert(!strcmp("ABC", msg.clipboard.text));

    device_msg_destroy(&msg);
}

static void test_deserialize_clipboard_big(void) {
    unsigned char input[DEVICE_MSG_MAX_SIZE];
    input[0] = DEVICE_MSG_TYPE_CLIPBOARD;
    input[1] = DEVICE_MSG_TEXT_MAX_LENGTH >> 8; // MSB
    input[2] = DEVICE_MSG_TEXT_MAX_LENGTH & 0xff; // LSB

    memset(input + 3, 'a', DEVICE_MSG_TEXT_MAX_LENGTH);

    struct device_msg msg;
    ssize_t r = device_msg_deserialize(input, sizeof(input), &msg);
    assert(r == DEVICE_MSG_MAX_SIZE);

    assert(msg.type == DEVICE_MSG_TYPE_CLIPBOARD);
    assert(msg.clipboard.text);
    assert(strlen(msg.clipboard.text) == DEVICE_MSG_TEXT_MAX_LENGTH);
    assert(msg.clipboard.text[0] == 'a');

    device_msg_destroy(&msg);
}

int main(void) {
    test_deserialize_clipboard();
    test_deserialize_clipboard_big();
    return 0;
}
