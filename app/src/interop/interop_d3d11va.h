#ifndef SC_INTEROP_D3D11VA_H
#define SC_INTEROP_D3D11VA_H

#include "common.h"

#include <SDL3/SDL.h>

#define COBJMACROS
#include <d3d11.h>

#include "coords.h"
#include "interop.h"

struct sc_interop_d3d11va {
    struct sc_interop interop; // interop trait

    SDL_Renderer *renderer; // owned by the screen
    ID3D11DeviceContext *device_ctx;

    // Only valid if interop.texture != NULL
    // The decoder surfaces may be larger than the frame for alignment purposes:
    // the texture has their size to receive a whole copy, and only the frame
    // area is rendered
    struct sc_size texture_size;
};

struct sc_interop_d3d11va *
sc_interop_d3d11va_new(SDL_Renderer *renderer);

#endif
