#ifndef SC_EGL_H
#define SC_EGL_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

// Use the EGL definitions bundled with SDL
#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS
#include <SDL3/SDL_egl.h>

#include "opengl.h"

// From GL_OES_EGL_image, only defined by the OpenGL ES headers (which cannot be
// included along with the OpenGL headers)
typedef void (APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
    (GLenum target, GLeglImageOES image);

struct sc_egl {
    EGLDisplay display;
    const char *extensions; // owned by EGL

    PFNEGLQUERYSTRINGPROC QueryString;
    PFNEGLGETERRORPROC GetError;

    // EGL_KHR_image_base (NULL if unsupported)
    PFNEGLCREATEIMAGEKHRPROC CreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC DestroyImageKHR;

    // GL_OES_EGL_image, the OpenGL side of EGLImage (NULL if unsupported)
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC EGLImageTargetTexture2DOES;

    // EGL_EXT_image_dma_buf_import
    bool has_dma_buf_import;
    // EGL_EXT_image_dma_buf_import_modifiers
    bool has_dma_buf_import_modifiers;
};

// Same value as DRM_FORMAT_MOD_INVALID from drm_fourcc.h, defined here to avoid
// depending on the libdrm headers
#define SC_DRM_FORMAT_MOD_INVALID UINT64_C(0x00FFFFFFFFFFFFFF)

struct sc_egl_dma_buf_plane {
    int fd;
    uint32_t offset;
    uint32_t pitch;

    // The DRM format modifier of the buffer (SC_DRM_FORMAT_MOD_INVALID if
    // unknown), only used if the EGL implementation supports modifiers
    uint64_t modifier;
};

bool
sc_egl_init(struct sc_egl *egl);

bool
sc_egl_has_extension(struct sc_egl *egl, const char *extension);

/**
 * Import a single-plane DMA-BUF as an EGLImage
 *
 * The DRM format is a FourCC (drm_fourcc.h) that describes the plane, for
 * example DRM_FORMAT_R8 or DRM_FORMAT_GR88.
 *
 * Return EGL_NO_IMAGE_KHR on error (see egl->GetError()).
 */
EGLImageKHR
sc_egl_create_image_dma_buf(struct sc_egl *egl, uint32_t drm_format,
                            int width, int height,
                            const struct sc_egl_dma_buf_plane *plane);

void
sc_egl_destroy_image(struct sc_egl *egl, EGLImageKHR image);

#endif
