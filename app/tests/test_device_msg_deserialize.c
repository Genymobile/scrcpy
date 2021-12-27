#include "common.h"

#include <assert.h>
#include <string.h>

#include "device_msg.h"

#include <stdio.h>

static void test_deserialize_clipboard(void) {
    const unsigned char input[] = {
        DEVICE_MSG_TYPE_CLIPBOARD,
        0x00, 0x00, 0x00, 0x03, // text length
        0x41, 0x42, 0x43, // "ABC"
    };

    struct device_msg msg;
    ssize_t r = device_msg_deserialize(input, sizeof(input), &msg);
    assert(r == 8);

    assert(msg.type == DEVICE_MSG_TYPE_CLIPBOARD);
    assert(msg.clipboard.text);
    assert(!strcmp("ABC", msg.clipboard.text));

    device_msg_destroy(&msg);
}

static void test_deserialize_clipboard_big(void) {
    unsigned char input[DEVICE_MSG_MAX_SIZE];
    input[0] = DEVICE_MSG_TYPE_CLIPBOARD;
    input[1] = (DEVICE_MSG_TEXT_MAX_LENGTH & 0xff000000u) >> 24;
    input[2] = (DEVICE_MSG_TEXT_MAX_LENGTH & 0x00ff0000u) >> 16;
    input[3] = (DEVICE_MSG_TEXT_MAX_LENGTH & 0x0000ff00u) >> 8;
    input[4] =  DEVICE_MSG_TEXT_MAX_LENGTH & 0x000000ffu;

    memset(input + 5, 'a', DEVICE_MSG_TEXT_MAX_LENGTH);

    struct device_msg msg;
    ssize_t r = device_msg_deserialize(input, sizeof(input), &msg);
    assert(r == DEVICE_MSG_MAX_SIZE);

    assert(msg.type == DEVICE_MSG_TYPE_CLIPBOARD);
    assert(msg.clipboard.text);
    assert(strlen(msg.clipboard.text) == DEVICE_MSG_TEXT_MAX_LENGTH);
    assert(msg.clipboard.text[0] == 'a');

    device_msg_destroy(&msg);
}

static void test_deserialize_ack_set_clipboard(void) {
    const unsigned char input[] = {
        DEVICE_MSG_TYPE_ACK_CLIPBOARD,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // sequence
    };

    struct device_msg msg;
    ssize_t r = device_msg_deserialize(input, sizeof(input), &msg);
    assert(r == 9);

    assert(msg.type == DEVICE_MSG_TYPE_ACK_CLIPBOARD);
    assert(msg.ack_clipboard.sequence == UINT64_C(0x0102030405060708));
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_deserialize_clipboard();
    test_deserialize_clipboard_big();
    test_deserialize_ack_set_clipboard();
    return 0;
}
