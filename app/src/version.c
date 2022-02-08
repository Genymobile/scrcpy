#include "version.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef HAVE_V4L2
# include <libavdevice/avdevice.h>
#endif

void
scrcpy_print_version(void) {
    printf("\ndependencies:\n");
    printf(" - SDL %d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
                                SDL_PATCHLEVEL);
    printf(" - libavcodec %d.%d.%d\n", LIBAVCODEC_VERSION_MAJOR,
                                       LIBAVCODEC_VERSION_MINOR,
                                       LIBAVCODEC_VERSION_MICRO);
    printf(" - libavformat %d.%d.%d\n", LIBAVFORMAT_VERSION_MAJOR,
                                        LIBAVFORMAT_VERSION_MINOR,
                                        LIBAVFORMAT_VERSION_MICRO);
    printf(" - libavutil %d.%d.%d\n", LIBAVUTIL_VERSION_MAJOR,
                                      LIBAVUTIL_VERSION_MINOR,
                                      LIBAVUTIL_VERSION_MICRO);
#ifdef HAVE_V4L2
    printf(" - libavdevice %d.%d.%d\n", LIBAVDEVICE_VERSION_MAJOR,
                                        LIBAVDEVICE_VERSION_MINOR,
                                        LIBAVDEVICE_VERSION_MICRO);
#endif
}
