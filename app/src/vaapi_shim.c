// Load libva and libdrm at runtime (static builds only)
//
// FFmpeg calls libva (VA-API) and libdrm directly. A static scrcpy binary
// linked with them would fail to start on a system where they are not
// installed, and a statically linked libva could not load drivers built for a
// newer libva (the driver entry point is versioned).
//
// Instead, this file defines the libva and libdrm functions referenced by
// FFmpeg, and forwards them to the system libraries loaded at runtime: the
// system libva always matches the installed drivers. If they cannot be loaded,
// the functions fail, so that creating the VA-API device fails and scrcpy
// falls back to software decoding.

#include "common.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>

#include <va/va.h>
#include <va/va_drm.h>
#include <xf86drm.h>

#include "util/log.h"

// X(name, ret, unavailable, params, args)
//   - unavailable: the value returned if the function could not be loaded
//
// Every libva/libdrm function referenced by FFmpeg must be listed here (a
// missing one is a link error).
#define SC_VA_FUNCS(X) \
    X(vaGetDisplayDRM, VADisplay, NULL, (int fd), (fd)) \
    X(vaInitialize, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, int *major_version, int *minor_version), \
      (dpy, major_version, minor_version)) \
    X(vaTerminate, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy), (dpy)) \
    X(vaErrorStr, const char *, "unknown", \
      (VAStatus error_status), (error_status)) \
    X(vaQueryVendorString, const char *, NULL, \
      (VADisplay dpy), (dpy)) \
    X(vaSetDriverName, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, char *driver_name), (dpy, driver_name)) \
    X(vaSetErrorCallback, VAMessageCallback, NULL, \
      (VADisplay dpy, VAMessageCallback callback, void *user_context), \
      (dpy, callback, user_context)) \
    X(vaSetInfoCallback, VAMessageCallback, NULL, \
      (VADisplay dpy, VAMessageCallback callback, void *user_context), \
      (dpy, callback, user_context)) \
    X(vaMaxNumProfiles, int, 0, (VADisplay dpy), (dpy)) \
    X(vaMaxNumImageFormats, int, 0, (VADisplay dpy), (dpy)) \
    X(vaQueryConfigProfiles, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAProfile *profile_list, int *num_profiles), \
      (dpy, profile_list, num_profiles)) \
    X(vaCreateConfig, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAProfile profile, VAEntrypoint entrypoint, \
       VAConfigAttrib *attrib_list, int num_attribs, VAConfigID *config_id), \
      (dpy, profile, entrypoint, attrib_list, num_attribs, config_id)) \
    X(vaDestroyConfig, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAConfigID config_id), (dpy, config_id)) \
    X(vaQuerySurfaceAttributes, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAConfigID config, VASurfaceAttrib *attrib_list, \
       unsigned int *num_attribs), \
      (dpy, config, attrib_list, num_attribs)) \
    X(vaCreateSurfaces, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, unsigned int format, unsigned int width, \
       unsigned int height, VASurfaceID *surfaces, unsigned int num_surfaces, \
       VASurfaceAttrib *attrib_list, unsigned int num_attribs), \
      (dpy, format, width, height, surfaces, num_surfaces, attrib_list, \
       num_attribs)) \
    X(vaDestroySurfaces, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VASurfaceID *surfaces, int num_surfaces), \
      (dpy, surfaces, num_surfaces)) \
    X(vaCreateContext, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAConfigID config_id, int picture_width, \
       int picture_height, int flag, VASurfaceID *render_targets, \
       int num_render_targets, VAContextID *context), \
      (dpy, config_id, picture_width, picture_height, flag, render_targets, \
       num_render_targets, context)) \
    X(vaDestroyContext, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAContextID context), (dpy, context)) \
    X(vaCreateBuffer, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAContextID context, VABufferType type, \
       unsigned int size, unsigned int num_elements, void *data, \
       VABufferID *buf_id), \
      (dpy, context, type, size, num_elements, data, buf_id)) \
    X(vaDestroyBuffer, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VABufferID buffer_id), (dpy, buffer_id)) \
    X(vaMapBuffer, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VABufferID buf_id, void **pbuf), (dpy, buf_id, pbuf)) \
    X(vaMapBuffer2, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VABufferID buf_id, void **pbuf, uint32_t flags), \
      (dpy, buf_id, pbuf, flags)) \
    X(vaUnmapBuffer, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VABufferID buf_id), (dpy, buf_id)) \
    X(vaAcquireBufferHandle, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VABufferID buf_id, VABufferInfo *buf_info), \
      (dpy, buf_id, buf_info)) \
    X(vaReleaseBufferHandle, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VABufferID buf_id), (dpy, buf_id)) \
    X(vaBeginPicture, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAContextID context, VASurfaceID render_target), \
      (dpy, context, render_target)) \
    X(vaRenderPicture, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAContextID context, VABufferID *buffers, \
       int num_buffers), \
      (dpy, context, buffers, num_buffers)) \
    X(vaEndPicture, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAContextID context), (dpy, context)) \
    X(vaSyncSurface, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VASurfaceID render_target), (dpy, render_target)) \
    X(vaExportSurfaceHandle, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VASurfaceID surface_id, uint32_t mem_type, \
       uint32_t flags, void *descriptor), \
      (dpy, surface_id, mem_type, flags, descriptor)) \
    X(vaQueryImageFormats, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAImageFormat *format_list, int *num_formats), \
      (dpy, format_list, num_formats)) \
    X(vaCreateImage, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAImageFormat *format, int width, int height, \
       VAImage *image), \
      (dpy, format, width, height, image)) \
    X(vaDestroyImage, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VAImageID image), (dpy, image)) \
    X(vaDeriveImage, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VASurfaceID surface, VAImage *image), \
      (dpy, surface, image)) \
    X(vaGetImage, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VASurfaceID surface, int x, int y, unsigned int width, \
       unsigned int height, VAImageID image), \
      (dpy, surface, x, y, width, height, image)) \
    X(vaPutImage, VAStatus, VA_STATUS_ERROR_UNIMPLEMENTED, \
      (VADisplay dpy, VASurfaceID surface, VAImageID image, int src_x, \
       int src_y, unsigned int src_width, unsigned int src_height, \
       int dest_x, int dest_y, unsigned int dest_width, \
       unsigned int dest_height), \
      (dpy, surface, image, src_x, src_y, src_width, src_height, dest_x, \
       dest_y, dest_width, dest_height)) \
    X(drmGetVersion, drmVersionPtr, NULL, (int fd), (fd)) \
    X(drmGetDevice, int, -ENOENT, \
      (int fd, drmDevicePtr *device), (fd, device)) \
    X(drmGetNodeTypeFromFd, int, -1, (int fd), (fd)) \
    X(drmGetRenderDeviceNameFromFd, char *, NULL, (int fd), (fd))

// Functions returning void: X(name, params, args)
#define SC_VA_VOID_FUNCS(X) \
    X(drmFreeVersion, (drmVersionPtr version), (version)) \
    X(drmFreeDevice, (drmDevicePtr *device), (device))

static struct {
#define SC_VA_DECL(name, ret, unavailable, params, args) ret (*name) params;
    SC_VA_FUNCS(SC_VA_DECL)
#undef SC_VA_DECL
#define SC_VA_DECL(name, params, args) void (*name) params;
    SC_VA_VOID_FUNCS(SC_VA_DECL)
#undef SC_VA_DECL
} sc_va;

static pthread_once_t sc_va_once = PTHREAD_ONCE_INIT;

static void *
sc_va_dlopen(const char *name) {
    void *lib = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        LOGW("Could not load %s (VA-API unavailable): %s", name, dlerror());
    }
    return lib;
}

static void *
sc_va_dlsym(void *libs[3], const char *name) {
    for (unsigned i = 0; i < 3; ++i) {
        void *sym = dlsym(libs[i], name);
        if (sym) {
            return sym;
        }
    }
    return NULL;
}

static void
sc_va_load(void) {
    void *libs[3] = {
        sc_va_dlopen("libva.so.2"),
        sc_va_dlopen("libva-drm.so.2"),
        sc_va_dlopen("libdrm.so.2"),
    };
    if (!libs[0] || !libs[1] || !libs[2]) {
        // All or nothing (the libraries are never unloaded)
        return;
    }

#define SC_VA_LOAD(name, ...) sc_va.name = sc_va_dlsym(libs, #name);
    SC_VA_FUNCS(SC_VA_LOAD)
    SC_VA_VOID_FUNCS(SC_VA_LOAD)
#undef SC_VA_LOAD

    LOGD("Loaded libva, libva-drm and libdrm");
}

// The entry points referenced by FFmpeg.
//
// The prototypes are declared first to satisfy -Wmissing-prototypes even when
// the system headers are too old to declare them (e.g. vaMapBuffer2).
//
// They are weak, so that if a static library providing them is linked anyway
// (for example libdrm.a pulled by another dependency), its definitions take
// precedence without conflict.
#define SC_VA_DEFINE(name, ret, unavailable, params, args) \
    ret name params; \
    __attribute__((weak)) ret name params { \
        pthread_once(&sc_va_once, sc_va_load); \
        if (!sc_va.name) { \
            return unavailable; \
        } \
        return sc_va.name args; \
    }
SC_VA_FUNCS(SC_VA_DEFINE)
#undef SC_VA_DEFINE

#define SC_VA_DEFINE(name, params, args) \
    void name params; \
    __attribute__((weak)) void name params { \
        pthread_once(&sc_va_once, sc_va_load); \
        if (sc_va.name) { \
            sc_va.name args; \
        } \
    }
SC_VA_VOID_FUNCS(SC_VA_DEFINE)
#undef SC_VA_DEFINE
