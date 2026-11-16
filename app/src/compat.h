#ifndef SC_COMPAT_H
#define SC_COMPAT_H

#include "config.h"

#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>
#include <SDL3/SDL_version.h>

#ifndef _WIN32
# define PRIu64_ PRIu64
# define SC_PRIsizet "zu"
#else
# define PRIu64_ "I64u"  // Windows...
# define SC_PRIsizet "Iu"
#endif

// In ffmpeg/doc/APIchanges:
// 2023-10-06 - 5432d2aacad - lavc 60.15.100 - avformat.h
//   Deprecate AVFormatContext.{nb_,}side_data, av_stream_add_side_data(),
//   av_stream_new_side_data(), and av_stream_get_side_data(). Side data fields
//   from AVFormatContext.codecpar should be used from now on.
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 15, 100)
# define SCRCPY_LAVC_HAS_CODECPAR_CODEC_SIDEDATA
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...);
#endif

#ifndef HAVE_VASPRINTF
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#ifndef HAVE_NRAND48
long nrand48(unsigned short xsubi[3]);
#endif

#ifndef HAVE_JRAND48
long jrand48(unsigned short xsubi[3]);
#endif

#ifndef HAVE_REALLOCARRAY
void *reallocarray(void *ptr, size_t nmemb, size_t size);
#endif

#endif
