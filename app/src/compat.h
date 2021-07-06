#ifndef COMPAT_H
#define COMPAT_H

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#ifdef __APPLE__
# define _DARWIN_C_SOURCE
#endif

#include <libavformat/version.h>
#include <SDL2/SDL_version.h>

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

#if SDL_VERSION_ATLEAST(2, 0, 5)
// <https://wiki.libsdl.org/SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH>
# define SCRCPY_SDL_HAS_HINT_MOUSE_FOCUS_CLICKTHROUGH
// <https://wiki.libsdl.org/SDL_GetDisplayUsableBounds>
# define SCRCPY_SDL_HAS_GET_DISPLAY_USABLE_BOUNDS
// <https://wiki.libsdl.org/SDL_WindowFlags>
# define SCRCPY_SDL_HAS_WINDOW_ALWAYS_ON_TOP
#endif

#if SDL_VERSION_ATLEAST(2, 0, 8)
// <https://hg.libsdl.org/SDL/rev/dfde5d3f9781>
# define SCRCPY_SDL_HAS_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
#endif

#ifndef HAVE_STRDUP
char *strdup(const char *s);
#endif

#endif
