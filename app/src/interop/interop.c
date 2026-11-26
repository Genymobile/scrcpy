#include "interop.h"

#include <assert.h>

#include "interop_software.h"
#ifdef HAVE_VAAPI
# include "interop_vaapi.h"
#endif
#ifdef HAVE_D3D11VA
# include "interop_d3d11va.h"
#endif
#ifdef HAVE_VIDEOTOOLBOX
# include "interop_videotoolbox.h"
#endif
#include "util/log.h"

static enum AVHWDeviceType
sc_interop_get_hw_type(enum sc_hwdec_mode hwdec_mode) {
    switch (hwdec_mode) {
        case SC_HWDEC_MODE_VAAPI:
            return AV_HWDEVICE_TYPE_VAAPI;
        case SC_HWDEC_MODE_D3D11VA:
            return AV_HWDEVICE_TYPE_D3D11VA;
        case SC_HWDEC_MODE_VIDEOTOOLBOX:
            return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        default:
            assert(hwdec_mode == SC_HWDEC_MODE_DISABLED);
            return AV_HWDEVICE_TYPE_NONE;
    }
}

struct sc_interop *
sc_interop_new(enum AVHWDeviceType hw_type, SDL_Renderer *renderer,
               struct sc_opengl *gl, bool mipmaps) {
    switch (hw_type) {
        case AV_HWDEVICE_TYPE_NONE: {
            struct sc_interop_software *sw =
                sc_interop_software_new(renderer, gl, mipmaps);
            return sw ? &sw->interop : NULL;
        }
#ifdef HAVE_VAAPI
        case AV_HWDEVICE_TYPE_VAAPI: {
            struct sc_interop_vaapi *vaapi =
                sc_interop_vaapi_new(renderer, gl, mipmaps);
            return vaapi ? &vaapi->interop : NULL;
        }
#endif
#ifdef HAVE_D3D11VA
        case AV_HWDEVICE_TYPE_D3D11VA: {
            struct sc_interop_d3d11va *d3d11va =
                sc_interop_d3d11va_new(renderer);
            return d3d11va ? &d3d11va->interop : NULL;
        }
#endif
#ifdef HAVE_VIDEOTOOLBOX
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: {
            struct sc_interop_videotoolbox *vt =
                sc_interop_videotoolbox_new(renderer);
            return vt ? &vt->interop : NULL;
        }
#endif
        default:
            LOGE("No interop for hardware device type: %s",
                 av_hwdevice_get_type_name(hw_type));
            return NULL;
    }
}

static inline const char *
sc_interop_get_hw_type_name(enum AVHWDeviceType hw_type) {
    return hw_type != AV_HWDEVICE_TYPE_NONE
         ? av_hwdevice_get_type_name(hw_type)
         : "software";
}

static struct sc_interop *
sc_interop_new_auto(SDL_Renderer *renderer, struct sc_opengl *gl,
                    bool mipmaps) {
    // In order of preference
    static const enum AVHWDeviceType hw_types[] = {
#ifdef HAVE_VAAPI
        AV_HWDEVICE_TYPE_VAAPI,
#endif
#ifdef HAVE_D3D11VA
        AV_HWDEVICE_TYPE_D3D11VA,
#endif
#ifdef HAVE_VIDEOTOOLBOX
        AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
#endif
        AV_HWDEVICE_TYPE_NONE, // software
    };

    for (size_t i = 0; i < ARRAY_LEN(hw_types); ++i) {
        enum AVHWDeviceType hw_type = hw_types[i];
        struct sc_interop *interop =
            sc_interop_new(hw_type, renderer, gl, mipmaps);
        if (interop) {
            return interop;
        }
        LOGI("Interop %s unavailable", sc_interop_get_hw_type_name(hw_type));
    }

    LOGE("No compatible interop");
    return NULL;
}

struct sc_interop *
sc_interop_new_from_hwdec_mode(enum sc_hwdec_mode hwdec_mode,
                               SDL_Renderer *renderer, struct sc_opengl *gl,
                               bool mipmaps) {
    if (hwdec_mode == SC_HWDEC_MODE_AUTO) {
        return sc_interop_new_auto(renderer, gl, mipmaps);
    }

    enum AVHWDeviceType hw_type = sc_interop_get_hw_type(hwdec_mode);
    struct sc_interop *interop = sc_interop_new(hw_type, renderer, gl, mipmaps);
    if (!interop) {
        LOGE("Interop %s unavailable", sc_interop_get_hw_type_name(hw_type));
    }
    return interop;
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
