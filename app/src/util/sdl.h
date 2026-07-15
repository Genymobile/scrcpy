#ifndef SC_SDL_H
#define SC_SDL_H

#include "common.h"

#include <stdint.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include "coords.h"

SDL_Window *
sc_sdl_create_window(const char *title, int64_t x, int64_t y, int64_t width,
                     int64_t height, int64_t flags);

// Configure an existing Cocoa window/view as the SDL render target.
// Pointers are __unsafe_unretained and owned by the application host.
void
sc_sdl_set_embedded_host(void *nswindow, void *nsview);

bool
sc_sdl_is_embedded(void);

struct sc_size
sc_sdl_get_window_size(SDL_Window *window);

void
sc_sdl_set_window_size(SDL_Window *window, struct sc_size size);

struct sc_point
sc_sdl_get_window_position(SDL_Window *window);

void
sc_sdl_set_window_position(SDL_Window *window, struct sc_point point);

void
sc_sdl_show_window(SDL_Window *window);

void
sc_sdl_hide_window(SDL_Window *window);

bool
sc_sdl_render_clear(SDL_Renderer *renderer);

void
sc_sdl_render_present(SDL_Renderer *renderer);

#endif
