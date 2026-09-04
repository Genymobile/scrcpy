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
// 2018-02-06 - 0694d87024 - lavf 58.9.100 - avformat.h
//   Deprecate use of av_register_input_format(), av_register_output_format(),
//   av_register_all(), av_iformat_next(), av_oformat_next().
//   Add av_demuxer_iterate(), and av_muxer_iterate().
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 9, 100)
# define SCRCPY_LAVF_HAS_NEW_MUXER_ITERATOR_API
#else
# define SCRCPY_LAVF_REQUIRES_REGISTER_ALL
#endif

// Not documented in ffmpeg/doc/APIchanges, but AV_CODEC_ID_AV1 has been added
// by FFmpeg commit d42809f9835a4e9e5c7c63210abb09ad0ef19cfb (included in tag
// n3.3).
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 89, 100)
# define SCRCPY_LAVC_HAS_AV1
#endif

// In ffmpeg/doc/APIchanges:
// 2018-01-28 - ea3672b7d6 - lavf 58.7.100 - avformat.h
//   Deprecate AVFormatContext filename field which had limited length, use the
//   new dynamically allocated url field instead.
//
// 2018-01-28 - ea3672b7d6 - lavf 58.7.100 - avformat.h
//   Add url field to AVFormatContext and add ff_format_set_url helper function.
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 7, 100)
# define SCRCPY_LAVF_HAS_AVFORMATCONTEXT_URL
#endif

// Not documented in ffmpeg/doc/APIchanges, but the channel_layout API
// has been replaced by chlayout in FFmpeg commit
// f423497b455da06c1337846902c770028760e094.
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 23, 100)
# define SCRCPY_LAVU_HAS_CHLAYOUT
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
