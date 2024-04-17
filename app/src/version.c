#include "version.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef HAVE_V4L2
# include <libavdevice/avdevice.h>
#endif
#ifdef HAVE_USB
# include <libusb-1.0/libusb.h>
#endif

void
scrcpy_print_version(void) {
    printf("\nDependencies (compiled / linked):\n");

    SDL_version sdl;
    SDL_GetVersion(&sdl);
    printf(" - SDL: %u.%u.%u / %u.%u.%u\n",
           SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
           (unsigned) sdl.major, (unsigned) sdl.minor, (unsigned) sdl.patch);

    unsigned avcodec = avcodec_version();
    printf(" - libavcodec: %u.%u.%u / %u.%u.%u\n",
           LIBAVCODEC_VERSION_MAJOR,
           LIBAVCODEC_VERSION_MINOR,
           LIBAVCODEC_VERSION_MICRO,
           AV_VERSION_MAJOR(avcodec),
           AV_VERSION_MINOR(avcodec),
           AV_VERSION_MICRO(avcodec));

    unsigned avformat = avformat_version();
    printf(" - libavformat: %u.%u.%u / %u.%u.%u\n",
           LIBAVFORMAT_VERSION_MAJOR,
           LIBAVFORMAT_VERSION_MINOR,
           LIBAVFORMAT_VERSION_MICRO,
           AV_VERSION_MAJOR(avformat),
           AV_VERSION_MINOR(avformat),
           AV_VERSION_MICRO(avformat));

    unsigned avutil = avutil_version();
    printf(" - libavutil: %u.%u.%u / %u.%u.%u\n",
           LIBAVUTIL_VERSION_MAJOR,
           LIBAVUTIL_VERSION_MINOR,
           LIBAVUTIL_VERSION_MICRO,
           AV_VERSION_MAJOR(avutil),
           AV_VERSION_MINOR(avutil),
           AV_VERSION_MICRO(avutil));

#ifdef HAVE_V4L2
    unsigned avdevice = avdevice_version();
    printf(" - libavdevice: %u.%u.%u / %u.%u.%u\n",
           LIBAVDEVICE_VERSION_MAJOR,
           LIBAVDEVICE_VERSION_MINOR,
           LIBAVDEVICE_VERSION_MICRO,
           AV_VERSION_MAJOR(avdevice),
           AV_VERSION_MINOR(avdevice),
           AV_VERSION_MICRO(avdevice));
#endif

#ifdef HAVE_USB
    const struct libusb_version *usb = libusb_get_version();
    // The compiled version may not be known
    printf(" - libusb: - / %u.%u.%u\n",
           (unsigned) usb->major, (unsigned) usb->minor, (unsigned) usb->micro);
#endif
}
