#ifndef SCREENCONTROL_H
#define SCREENCONTROL_H

#include "common.h"
#include "controller.h"
#include "screen.h"

void screencontrol_handle_text_input(struct controller *controller,
                                     struct screen *screen,
                                     const SDL_TextInputEvent *event);
void screencontrol_handle_key(struct controller *controller,
                              struct screen *screen,
                              const SDL_KeyboardEvent *event);
void screencontrol_handle_mouse_motion(struct controller *controller,
                                       struct screen *screen,
                                       const SDL_MouseMotionEvent *event);
void screencontrol_handle_mouse_button(struct controller *controller,
                                       struct screen *screen,
                                       const SDL_MouseButtonEvent *event);
void screencontrol_handle_mouse_wheel(struct controller *controller,
                                      struct screen *screen,
                                      const SDL_MouseWheelEvent *event);


#endif
