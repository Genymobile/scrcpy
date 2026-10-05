#ifndef SC_INTEROP_VIDEOTOOLBOX_H
#define SC_INTEROP_VIDEOTOOLBOX_H

#include "common.h"

#include <SDL3/SDL.h>

#include "interop.h"

struct sc_interop_videotoolbox {
    struct sc_interop interop; // interop trait

    SDL_Renderer *renderer; // owned by the screen

    // The texture wraps the CVPixelBuffer of one frame, so a new texture is
    // created for every frame
};

struct sc_interop_videotoolbox *
sc_interop_videotoolbox_new(SDL_Renderer *renderer);

#endif
