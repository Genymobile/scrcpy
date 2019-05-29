#include <assert.h>
#include <string.h>

#include "device_event.h"

#include <stdio.h>
static void test_deserialize_clipboard_event(void) {
    const unsigned char input[] = {
        0x00, // DEVICE_EVENT_TYPE_CLIPBOARD
        0x00, 0x03, // text length
        0x41, 0x42, 0x43, // "ABC"
    };

    struct device_event event;
    ssize_t r = device_event_deserialize(input, sizeof(input), &event);
    assert(r == 6);

    assert(event.type == DEVICE_EVENT_TYPE_GET_CLIPBOARD);
    assert(event.clipboard_event.text);
    assert(!strcmp("ABC", event.clipboard_event.text));

    device_event_destroy(&event);
}

int main(void) {
    test_deserialize_clipboard_event();
    return 0;
}
