#include "interop.h"

#include <assert.h>

#include "interop_software.h"
#ifdef HAVE_VAAPI
# include "interop_vaapi.h"
#endif
#ifdef HAVE_D3D11VA
# include "interop_d3d11va.h"
#endif
#include "util/log.h"

struct sc_interop *
sc_interop_new(enum sc_hwdec_mode hwdec_mode, SDL_Renderer *renderer,
               struct sc_opengl *gl, bool mipmaps) {
    bool any = hwdec_mode == SC_HWDEC_MODE_AUTO;

#ifdef HAVE_VAAPI
    if (any || hwdec_mode == SC_HWDEC_MODE_VAAPI) {
        struct sc_interop_vaapi *vaapi =
            sc_interop_vaapi_new(renderer, gl, mipmaps);
        if (vaapi) {
            return &vaapi->interop;
        }
        if (!any) {
            LOGE("VA-API interop unavailable");
            return NULL;
        }
        LOGI("VA-API interop unavailable");
    }
#endif

#ifdef HAVE_D3D11VA
    if (any || hwdec_mode == SC_HWDEC_MODE_D3D11VA) {
        struct sc_interop_d3d11va *d3d11va = sc_interop_d3d11va_new(renderer);
        if (d3d11va) {
            return &d3d11va->interop;
        }
        if (!any) {
            LOGE("D3D11VA interop unavailable");
            return NULL;
        }
        LOGI("D3D11VA interop unavailable");
    }
#endif

    if (any || hwdec_mode == SC_HWDEC_MODE_DISABLED) {
        struct sc_interop_software *sw =
            sc_interop_software_new(renderer, gl, mipmaps);
        if (sw) {
            return &sw->interop;
        }
        LOGI("Software interop failed");
    }

    LOGE("No compatible interop");
    return NULL;
}

void
sc_interop_delete(struct sc_interop *interop) {
    assert(interop->ops && interop->ops->destroy);
    interop->ops->destroy(interop);
    free(interop);
}

SDL_Colorspace
sc_interop_to_sdl_color_space(enum AVColorSpace color_space,
                              enum AVColorRange color_range) {
    bool full_range = color_range == AVCOL_RANGE_JPEG;

    switch (color_space) {
        case AVCOL_SPC_BT709:
        case AVCOL_SPC_RGB:
        case AVCOL_SPC_UNSPECIFIED:
        case AVCOL_SPC_YCGCO:
            return full_range ? SDL_COLORSPACE_BT709_FULL
                              : SDL_COLORSPACE_BT709_LIMITED;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M:
            return full_range ? SDL_COLORSPACE_BT601_FULL
                              : SDL_COLORSPACE_BT601_LIMITED;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL:
            return full_range ? SDL_COLORSPACE_BT2020_FULL
                              : SDL_COLORSPACE_BT2020_LIMITED;
        default:
            return SDL_COLORSPACE_JPEG;
    }
}
