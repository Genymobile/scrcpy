#include "event_converter.h"

#define MAP(FROM, TO) case FROM: *to = TO; return true
#define FAIL default: return false

enum android_motionevent_buttons
convert_mouse_buttons(uint32_t state) {
    enum android_motionevent_buttons buttons = 0;
    if (state & SDL_BUTTON_LMASK) {
        buttons |= AMOTION_EVENT_BUTTON_PRIMARY;
    }
    if (state & SDL_BUTTON_RMASK) {
        buttons |= AMOTION_EVENT_BUTTON_SECONDARY;
    }
    if (state & SDL_BUTTON_MMASK) {
        buttons |= AMOTION_EVENT_BUTTON_TERTIARY;
    }
    if (state & SDL_BUTTON_X1MASK) {
        buttons |= AMOTION_EVENT_BUTTON_BACK;
    }
    if (state & SDL_BUTTON_X2MASK) {
        buttons |= AMOTION_EVENT_BUTTON_FORWARD;
    }
    return buttons;
}

bool
convert_mouse_action(SDL_EventType from, enum android_motionevent_action *to) {
    switch (from) {
        MAP(SDL_MOUSEBUTTONDOWN, AMOTION_EVENT_ACTION_DOWN);
        MAP(SDL_MOUSEBUTTONUP,   AMOTION_EVENT_ACTION_UP);
        FAIL;
    }
}

bool
convert_touch_action(SDL_EventType from, enum android_motionevent_action *to) {
    switch (from) {
        MAP(SDL_FINGERMOTION, AMOTION_EVENT_ACTION_MOVE);
        MAP(SDL_FINGERDOWN,   AMOTION_EVENT_ACTION_DOWN);
        MAP(SDL_FINGERUP,     AMOTION_EVENT_ACTION_UP);
        FAIL;
    }
}
