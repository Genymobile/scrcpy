#ifndef COMPAT_H
#define COMPAT_H

#include <libavformat/version.h>
#include <SDL2/SDL_version.h>

// In ffmpeg/doc/APIchanges:
// 2016-04-11 - 6f69f7a / 9200514 - lavf 57.33.100 / 57.5.0 - avformat.h
//   Add AVStream.codecpar, deprecate AVStream.codec.
#if    (LIBAVFORMAT_VERSION_MICRO >= 100 /* FFmpeg */ && \
        LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 33, 100)) \
    || (LIBAVFORMAT_VERSION_MICRO < 100 && /* Libav */ \
        LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 5, 0))
# define SCRCPY_LAVF_HAS_NEW_CODEC_PARAMS_API
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

// In ffmpeg/doc/APIchanges:
// 2016-04-21 - 7fc329e - lavc 57.37.100 - avcodec.h
//   Add a new audio/video encoding and decoding API with decoupled input
//   and output -- avcodec_send_packet(), avcodec_receive_frame(),
//   avcodec_send_frame() and avcodec_receive_packet().
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100)
# define SCRCPY_LAVF_HAS_NEW_ENCODING_DECODING_API
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

#endif
