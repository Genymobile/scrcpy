#ifndef SC_INTEROP_SOFTWARE_H
#define SC_INTEROP_SOFTWARE_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

#include "interop.h"
#include "opengl.h"

struct sc_interop_software {
    struct sc_interop interop; // interop trait

    SDL_Renderer *renderer; // owned by the screen

    struct sc_opengl *gl;
    bool mipmaps;
    uint32_t texture_ids[3]; // only set if mipmaps is enabled
};

struct sc_interop_software *
sc_interop_software_new(SDL_Renderer *renderer, struct sc_opengl *gl,
                        bool mipmaps);

#endif
