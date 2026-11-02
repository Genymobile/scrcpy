#ifndef SC_NAV_BAR_H
#define SC_NAV_BAR_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

#include "controller.h"

// Height of the navigation bar, in window coordinates (logical pixels)
#define SC_NAV_BAR_HEIGHT 40

enum sc_nav_bar_button {
    SC_NAV_BAR_BUTTON_VOLUME_DOWN,
    SC_NAV_BAR_BUTTON_VOLUME_UP,
    SC_NAV_BAR_BUTTON_BACK,
    SC_NAV_BAR_BUTTON_HOME,
    SC_NAV_BAR_BUTTON_APP_SWITCH,
    SC_NAV_BAR_BUTTON_ROTATE,
    SC_NAV_BAR_BUTTON_POWER,
    SC_NAV_BAR_BUTTON_COUNT,
    SC_NAV_BAR_BUTTON_NONE = -1,
};

/**
 * Bar of buttons (Back, Home, Recent apps, volume, rotation, power) displayed
 * at the bottom of the window, to control the device with the mouse.
 *
 * A button is pressed on the device while the mouse button is pressed (so that
 * long presses work), like a physical key.
 */
struct sc_nav_bar {
    struct sc_controller *controller;

    // The bar rectangle, in window coordinates
    SDL_FRect rect;

    enum sc_nav_bar_button hovered;
    enum sc_nav_bar_button pressed;
    bool pressed_by_touch;

    // Mouse buttons pressed on the bar (SDL_BUTTON_MASK()), whose events
    // belong to the bar until they are released
    uint32_t owned_buttons;
    // Finger pressed on the bar, if any
    bool touch_active;
    SDL_FingerID touch_finger;
};

void
sc_nav_bar_init(struct sc_nav_bar *bar, struct sc_controller *controller);

void
sc_nav_bar_set_rect(struct sc_nav_bar *bar, const SDL_FRect *rect);

// scale: the window pixel density
void
sc_nav_bar_render(struct sc_nav_bar *bar, SDL_Renderer *renderer, float scale);

/**
 * Handle a mouse, touch or window event
 *
 * Return true if the event was consumed by the bar (it must not be forwarded
 * to the device as a mouse or touch event). Set *render to true if the bar
 * must be rendered again.
 *
 * If paused is true, no new button press is sent to the device.
 */
bool
sc_nav_bar_handle_event(struct sc_nav_bar *bar, const SDL_Event *event,
                        bool paused, bool *render);

// Reset the bar state without sending anything to the device (e.g. once the
// device is disconnected)
void
sc_nav_bar_reset(struct sc_nav_bar *bar);

#endif
