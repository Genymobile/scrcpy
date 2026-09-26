#include "egl.h"

#include <assert.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "util/log.h"

bool
sc_egl_init(struct sc_egl *egl) {
    egl->display = (EGLDisplay) SDL_EGL_GetCurrentDisplay();
    if (egl->display == EGL_NO_DISPLAY) {
        LOGD("EGL_NO_DISPLAY");
        return false;
    }

    egl->QueryString = (PFNEGLQUERYSTRINGPROC)
        SDL_EGL_GetProcAddress("eglQueryString");
    assert(egl->QueryString);

    egl->GetError = (PFNEGLGETERRORPROC)
        SDL_EGL_GetProcAddress("eglGetError");
    assert(egl->GetError);

    egl->extensions = egl->QueryString(egl->display, EGL_EXTENSIONS);
    if (!egl->extensions) {
        LOGE("EGL error: Could not get EGL extensions");
        return false;
    }

    // eglGetProcAddress() may return a pointer even for unsupported functions,
    // so only resolve the entry points of advertised extensions
    egl->CreateImageKHR = NULL;
    egl->DestroyImageKHR = NULL;
    if (sc_egl_has_extension(egl, "EGL_KHR_image_base")
            || sc_egl_has_extension(egl, "EGL_KHR_image")) {
        egl->CreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)
            SDL_EGL_GetProcAddress("eglCreateImageKHR");
        assert(egl->CreateImageKHR);

        egl->DestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)
            SDL_EGL_GetProcAddress("eglDestroyImageKHR");
        assert(egl->DestroyImageKHR);
    }

    egl->EGLImageTargetTexture2DOES = NULL;
    if (SDL_GL_ExtensionSupported("GL_OES_EGL_image")) {
        egl->EGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
            SDL_GL_GetProcAddress("glEGLImageTargetTexture2DOES");
    }

    egl->has_dma_buf_import =
        sc_egl_has_extension(egl, "EGL_EXT_image_dma_buf_import");
    egl->has_dma_buf_import_modifiers =
        sc_egl_has_extension(egl, "EGL_EXT_image_dma_buf_import_modifiers");

    return true;
}

bool
sc_egl_has_extension(struct sc_egl *egl, const char *extension) {
    assert(egl->extensions);
    assert(extension);
    assert(!strchr(extension, ' '));

    size_t len = strlen(extension);
    const char *p = egl->extensions;
    while ((p = strstr(p, extension))) {
        if ((p == egl->extensions || p[-1] == ' ')
                && (p[len] == '\0' || p[len] == ' ')) {
            return true;
        }
        p += len;
    }

    return false;
}

EGLImageKHR
sc_egl_create_image_dma_buf(struct sc_egl *egl, uint32_t drm_format,
                            int width, int height,
                            const struct sc_egl_dma_buf_plane *plane) {
    assert(egl->CreateImageKHR);
    assert(egl->has_dma_buf_import);

    EGLint attrs[20];
    unsigned n = 0;
    attrs[n++] = EGL_WIDTH;
    attrs[n++] = width;
    attrs[n++] = EGL_HEIGHT;
    attrs[n++] = height;
    attrs[n++] = EGL_LINUX_DRM_FOURCC_EXT;
    attrs[n++] = (EGLint) drm_format;
    attrs[n++] = EGL_DMA_BUF_PLANE0_FD_EXT;
    attrs[n++] = plane->fd;
    attrs[n++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attrs[n++] = (EGLint) plane->offset;
    attrs[n++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attrs[n++] = (EGLint) plane->pitch;

    if (plane->modifier != SC_DRM_FORMAT_MOD_INVALID
            && egl->has_dma_buf_import_modifiers) {
        attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
        attrs[n++] = (EGLint) (plane->modifier & UINT32_MAX);
        attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
        attrs[n++] = (EGLint) (plane->modifier >> 32);
    }

    attrs[n++] = EGL_NONE;
    assert(n <= ARRAY_LEN(attrs));

    return egl->CreateImageKHR(egl->display, EGL_NO_CONTEXT,
                               EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
}

void
sc_egl_destroy_image(struct sc_egl *egl, EGLImageKHR image) {
    assert(egl->DestroyImageKHR);
    egl->DestroyImageKHR(egl->display, image);
}
