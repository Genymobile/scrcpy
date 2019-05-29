#include "device_event.h"

#include <string.h>
#include <SDL2/SDL_assert.h>

#include "buffer_util.h"
#include "log.h"

ssize_t
device_event_deserialize(const unsigned char *buf, size_t len,
                         struct device_event *event) {
    if (len < 3) {
        // at least type + empty string length
        return 0; // not available
    }

    event->type = buf[0];
    switch (event->type) {
        case DEVICE_EVENT_TYPE_GET_CLIPBOARD: {
            uint16_t clipboard_len = buffer_read16be(&buf[1]);
            if (clipboard_len > len - 3) {
                return 0; // not available
            }
            char *text = SDL_malloc(clipboard_len + 1);
            if (!text) {
                LOGW("Could not allocate text for clipboard");
                return -1;
            }
            if (clipboard_len) {
                memcpy(text, &buf[3], clipboard_len);
            }
            text[clipboard_len] = '\0';

            event->clipboard_event.text = text;
            return 3 + clipboard_len;
        }
        default:
            LOGW("Unsupported device event type: %d", (int) event->type);
            return -1; // error, we cannot recover
    }
}

void
device_event_destroy(struct device_event *event) {
    if (event->type == DEVICE_EVENT_TYPE_GET_CLIPBOARD) {
        SDL_free(event->clipboard_event.text);
    }
}
