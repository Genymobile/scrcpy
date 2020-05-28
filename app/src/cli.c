#include "cli.h"

#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>

#include "config.h"
#include "recorder.h"
#include "util/log.h"
#include "util/str_util.h"

void
scrcpy_print_usage(const char *arg0) {
#ifdef __APPLE__
# define CTRL_OR_CMD "Cmd"
#else
# define CTRL_OR_CMD "Ctrl"
#endif
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "\n"
        "    --always-on-top\n"
        "        Make scrcpy window always on top (above other windows).\n"
        "\n"
        "    -b, --bit-rate value\n"
        "        Encode the video at the given bit-rate, expressed in bits/s.\n"
        "        Unit suffixes are supported: 'K' (x1000) and 'M' (x1000000).\n"
        "        Default is %d.\n"
        "\n"
        "    --codec-options key[:type]=value[,...]\n"
        "        Set a list of comma-separated key:type=value options for the\n"
        "        device encoder.\n"
        "        The possible values for 'type' are 'int' (default), 'long',\n"
        "        'float' and 'string'.\n"
        "        The list of possible codec options is available in the\n"
        "        Android documentation:\n"
        "        <https://d.android.com/reference/android/media/MediaFormat>\n"
        "\n"
        "    --crop width:height:x:y\n"
        "        Crop the device screen on the server.\n"
        "        The values are expressed in the device natural orientation\n"
        "        (typically, portrait for a phone, landscape for a tablet).\n"
        "        Any --max-size value is computed on the cropped size.\n"
        "\n"
        "    --display id\n"
        "        Specify the display id to mirror.\n"
        "\n"
        "        The list of possible display ids can be listed by:\n"
        "            adb shell dumpsys display\n"
        "        (search \"mDisplayId=\" in the output)\n"
        "\n"
        "        Default is 0.\n"
        "\n"
        "    --force-adb-forward\n"
        "        Do not attempt to use \"adb reverse\" to connect to the\n"
        "        the device.\n"
        "\n"
        "    -f, --fullscreen\n"
        "        Start in fullscreen.\n"
        "\n"
        "    -h, --help\n"
        "        Print this help.\n"
        "\n"
        "    --lock-video-orientation value\n"
        "        Lock video orientation to value.\n"
        "        Possible values are -1 (unlocked), 0, 1, 2 and 3.\n"
        "        Natural device orientation is 0, and each increment adds a\n"
        "        90 degrees rotation counterclockwise.\n"
        "        Default is %d%s.\n"
        "\n"
        "    --max-fps value\n"
        "        Limit the frame rate of screen capture (officially supported\n"
        "        since Android 10, but may work on earlier versions).\n"
        "\n"
        "    -m, --max-size value\n"
        "        Limit both the width and height of the video to value. The\n"
        "        other dimension is computed so that the device aspect-ratio\n"
        "        is preserved.\n"
        "        Default is %d%s.\n"
        "\n"
        "    -n, --no-control\n"
        "        Disable device control (mirror the device in read-only).\n"
        "\n"
        "    -N, --no-display\n"
        "        Do not display device (only when screen recording is\n"
        "        enabled).\n"
        "\n"
        "    --no-mipmaps\n"
        "        If the renderer is OpenGL 3.0+ or OpenGL ES 2.0+, then\n"
        "        mipmaps are automatically generated to improve downscaling\n"
        "        quality. This option disables the generation of mipmaps.\n"
        "\n"
        "    -p, --port port[:port]\n"
        "        Set the TCP port (range) used by the client to listen.\n"
        "        Default is %d:%d.\n"
        "\n"
        "    --prefer-text\n"
        "        Inject alpha characters and space as text events instead of\n"
        "        key events.\n"
        "        This avoids issues when combining multiple keys to enter a\n"
        "        special character, but breaks the expected behavior of alpha\n"
        "        keys in games (typically WASD).\n"
        "\n"
        "    --push-target path\n"
        "        Set the target directory for pushing files to the device by\n"
        "        drag & drop. It is passed as-is to \"adb push\".\n"
        "        Default is \"/sdcard/\".\n"
        "\n"
        "    -r, --record file.mp4\n"
        "        Record screen to file.\n"
        "        The format is determined by the --record-format option if\n"
        "        set, or by the file extension (.mp4 or .mkv).\n"
        "\n"
        "    --record-format format\n"
        "        Force recording format (either mp4 or mkv).\n"
        "\n"
        "    --render-driver name\n"
        "        Request SDL to use the given render driver (this is just a\n"
        "        hint).\n"
        "        Supported names are currently \"direct3d\", \"opengl\",\n"
        "        \"opengles2\", \"opengles\", \"metal\" and \"software\".\n"
        "        <https://wiki.libsdl.org/SDL_HINT_RENDER_DRIVER>\n"
        "\n"
        "    --render-expired-frames\n"
        "        By default, to minimize latency, scrcpy always renders the\n"
        "        last available decoded frame, and drops any previous ones.\n"
        "        This flag forces to render all frames, at a cost of a\n"
        "        possible increased latency.\n"
        "\n"
        "    --rotation value\n"
        "        Set the initial display rotation.\n"
        "        Possibles values are 0, 1, 2 and 3. Each increment adds a 90\n"
        "        degrees rotation counterclockwise.\n"
        "\n"
        "    -s, --serial serial\n"
        "        The device serial number. Mandatory only if several devices\n"
        "        are connected to adb.\n"
        "\n"
        "    -S, --turn-screen-off\n"
        "        Turn the device screen off immediately.\n"
        "\n"
        "    -t, --show-touches\n"
        "        Enable \"show touches\" on start, restore the initial value\n"
        "        on exit.\n"
        "        It only shows physical touches (not clicks from scrcpy).\n"
        "\n"
        "    -v, --version\n"
        "        Print the version of scrcpy.\n"
        "\n"
        "    -V, --verbosity value\n"
        "        Set the log level (debug, info, warn or error).\n"
#ifndef NDEBUG
        "        Default is debug.\n"
#else
        "        Default is info.\n"
#endif
        "\n"
        "    -w, --stay-awake\n"
        "        Keep the device on while scrcpy is running, when the device\n"
        "        is plugged in.\n"
        "\n"
        "    --window-borderless\n"
        "        Disable window decorations (display borderless window).\n"
        "\n"
        "    --window-title text\n"
        "        Set a custom window title.\n"
        "\n"
        "    --window-x value\n"
        "        Set the initial window horizontal position.\n"
        "        Default is \"auto\".\n"
        "\n"
        "    --window-y value\n"
        "        Set the initial window vertical position.\n"
        "        Default is \"auto\".\n"
        "\n"
        "    --window-width value\n"
        "        Set the initial window width.\n"
        "        Default is 0 (automatic).\n"
        "\n"
        "    --window-height value\n"
        "        Set the initial window width.\n"
        "        Default is 0 (automatic).\n"
        "\n"
        "Shortcuts:\n"
        "\n"
        "    " CTRL_OR_CMD "+f\n"
        "        Switch fullscreen mode\n"
        "\n"
        "    " CTRL_OR_CMD "+Left\n"
        "        Rotate display left\n"
        "\n"
        "    " CTRL_OR_CMD "+Right\n"
        "        Rotate display right\n"
        "\n"
        "    " CTRL_OR_CMD "+g\n"
        "        Resize window to 1:1 (pixel-perfect)\n"
        "\n"
        "    " CTRL_OR_CMD "+x\n"
        "    Double-click on black borders\n"
        "        Resize window to remove black borders\n"
        "\n"
        "    Ctrl+h\n"
        "    Middle-click\n"
        "        Click on HOME\n"
        "\n"
        "    " CTRL_OR_CMD "+b\n"
        "    " CTRL_OR_CMD "+Backspace\n"
        "    Right-click (when screen is on)\n"
        "        Click on BACK\n"
        "\n"
        "    " CTRL_OR_CMD "+s\n"
        "        Click on APP_SWITCH\n"
        "\n"
        "    Ctrl+m\n"
        "        Click on MENU\n"
        "\n"
        "    " CTRL_OR_CMD "+Up\n"
        "        Click on VOLUME_UP\n"
        "\n"
        "    " CTRL_OR_CMD "+Down\n"
        "        Click on VOLUME_DOWN\n"
        "\n"
        "    " CTRL_OR_CMD "+p\n"
        "        Click on POWER (turn screen on/off)\n"
        "\n"
        "    Right-click (when screen is off)\n"
        "        Power on\n"
        "\n"
        "    " CTRL_OR_CMD "+o\n"
        "        Turn device screen off (keep mirroring)\n"
        "\n"
        "    " CTRL_OR_CMD "+Shift+o\n"
        "        Turn device screen on\n"
        "\n"
        "    " CTRL_OR_CMD "+r\n"
        "        Rotate device screen\n"
        "\n"
        "    " CTRL_OR_CMD "+n\n"
        "        Expand notification panel\n"
        "\n"
        "    " CTRL_OR_CMD "+Shift+n\n"
        "        Collapse notification panel\n"
        "\n"
        "    " CTRL_OR_CMD "+c\n"
        "        Copy device clipboard to computer\n"
        "\n"
        "    " CTRL_OR_CMD "+v\n"
        "        Paste computer clipboard to device\n"
        "\n"
        "    " CTRL_OR_CMD "+Shift+v\n"
        "        Copy computer clipboard to device (and paste if the device\n"
        "        runs Android >= 7)\n"
        "\n"
        "    " CTRL_OR_CMD "+i\n"
        "        Enable/disable FPS counter (print frames/second in logs)\n"
        "\n"
        "    Drag & drop APK file\n"
        "        Install APK from computer\n"
        "\n",
        arg0,
        DEFAULT_BIT_RATE,
        DEFAULT_LOCK_VIDEO_ORIENTATION, DEFAULT_LOCK_VIDEO_ORIENTATION >= 0 ? "" : " (unlocked)",
        DEFAULT_MAX_SIZE, DEFAULT_MAX_SIZE ? "" : " (unlimited)",
        DEFAULT_LOCAL_PORT_RANGE_FIRST, DEFAULT_LOCAL_PORT_RANGE_LAST);
}

static bool
parse_integer_arg(const char *s, long *out, bool accept_suffix, long min,
                  long max, const char *name) {
    long value;
    bool ok;
    if (accept_suffix) {
        ok = parse_integer_with_suffix(s, &value);
    } else {
        ok = parse_integer(s, &value);
    }
    if (!ok) {
        LOGE("Could not parse %s: %s", name, s);
        return false;
    }

    if (value < min || value > max) {
        LOGE("Could not parse %s: value (%ld) out-of-range (%ld; %ld)",
             name, value, min, max);
        return false;
    }

    *out = value;
    return true;
}

static size_t
parse_integers_arg(const char *s, size_t max_items, long *out, long min,
                   long max, const char *name) {
    size_t count = parse_integers(s, ':', max_items, out);
    if (!count) {
        LOGE("Could not parse %s: %s", name, s);
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        long value = out[i];
        if (value < min || value > max) {
            LOGE("Could not parse %s: value (%ld) out-of-range (%ld; %ld)",
                 name, value, min, max);
            return 0;
        }
    }

    return count;
}

static bool
parse_bit_rate(const char *s, uint32_t *bit_rate) {
    long value;
    // long may be 32 bits (it is the case on mingw), so do not use more than
    // 31 bits (long is signed)
    bool ok = parse_integer_arg(s, &value, true, 0, 0x7FFFFFFF, "bit-rate");
    if (!ok) {
        return false;
    }

    *bit_rate = (uint32_t) value;
    return true;
}

static bool
parse_max_size(const char *s, uint16_t *max_size) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 0xFFFF, "max size");
    if (!ok) {
        return false;
    }

    *max_size = (uint16_t) value;
    return true;
}

static bool
parse_max_fps(const char *s, uint16_t *max_fps) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 1000, "max fps");
    if (!ok) {
        return false;
    }

    *max_fps = (uint16_t) value;
    return true;
}

static bool
parse_lock_video_orientation(const char *s, int8_t *lock_video_orientation) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, -1, 3,
                                "lock video orientation");
    if (!ok) {
        return false;
    }

    *lock_video_orientation = (int8_t) value;
    return true;
}

static bool
parse_rotation(const char *s, uint8_t *rotation) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 3, "rotation");
    if (!ok) {
        return false;
    }

    *rotation = (uint8_t) value;
    return true;
}

static bool
parse_window_position(const char *s, int16_t *position) {
    // special value for "auto"
    static_assert(WINDOW_POSITION_UNDEFINED == -0x8000, "unexpected value");

    if (!strcmp(s, "auto")) {
        *position = WINDOW_POSITION_UNDEFINED;
        return true;
    }

    long value;
    bool ok = parse_integer_arg(s, &value, false, -0x7FFF, 0x7FFF,
                                "window position");
    if (!ok) {
        return false;
    }

    *position = (int16_t) value;
    return true;
}

static bool
parse_window_dimension(const char *s, uint16_t *dimension) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 0xFFFF,
                                "window dimension");
    if (!ok) {
        return false;
    }

    *dimension = (uint16_t) value;
    return true;
}

static bool
parse_port_range(const char *s, struct port_range *port_range) {
    long values[2];
    size_t count = parse_integers_arg(s, 2, values, 0, 0xFFFF, "port");
    if (!count) {
        return false;
    }

    uint16_t v0 = (uint16_t) values[0];
    if (count == 1) {
        port_range->first = v0;
        port_range->last = v0;
        return true;
    }

    assert(count == 2);
    uint16_t v1 = (uint16_t) values[1];
    if (v0 < v1) {
        port_range->first = v0;
        port_range->last = v1;
    } else {
        port_range->first = v1;
        port_range->last = v0;
    }

    return true;
}

static bool
parse_display_id(const char *s, uint16_t *display_id) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 0xFFFF, "display id");
    if (!ok) {
        return false;
    }

    *display_id = (uint16_t) value;
    return true;
}

static bool
parse_log_level(const char *s, enum sc_log_level *log_level) {
    if (!strcmp(s, "debug")) {
        *log_level = SC_LOG_LEVEL_DEBUG;
        return true;
    }

    if (!strcmp(s, "info")) {
        *log_level = SC_LOG_LEVEL_INFO;
        return true;
    }

    if (!strcmp(s, "warn")) {
        *log_level = SC_LOG_LEVEL_WARN;
        return true;
    }

    if (!strcmp(s, "error")) {
        *log_level = SC_LOG_LEVEL_ERROR;
        return true;
    }

    LOGE("Could not parse log level: %s", s);
    return false;
}

static bool
parse_record_format(const char *optarg, enum recorder_format *format) {
    if (!strcmp(optarg, "mp4")) {
        *format = RECORDER_FORMAT_MP4;
        return true;
    }
    if (!strcmp(optarg, "mkv")) {
        *format = RECORDER_FORMAT_MKV;
        return true;
    }
    LOGE("Unsupported format: %s (expected mp4 or mkv)", optarg);
    return false;
}

static enum recorder_format
guess_record_format(const char *filename) {
    size_t len = strlen(filename);
    if (len < 4) {
        return 0;
    }
    const char *ext = &filename[len - 4];
    if (!strcmp(ext, ".mp4")) {
        return RECORDER_FORMAT_MP4;
    }
    if (!strcmp(ext, ".mkv")) {
        return RECORDER_FORMAT_MKV;
    }
    return 0;
}

#define OPT_RENDER_EXPIRED_FRAMES  1000
#define OPT_WINDOW_TITLE           1001
#define OPT_PUSH_TARGET            1002
#define OPT_ALWAYS_ON_TOP          1003
#define OPT_CROP                   1004
#define OPT_RECORD_FORMAT          1005
#define OPT_PREFER_TEXT            1006
#define OPT_WINDOW_X               1007
#define OPT_WINDOW_Y               1008
#define OPT_WINDOW_WIDTH           1009
#define OPT_WINDOW_HEIGHT          1010
#define OPT_WINDOW_BORDERLESS      1011
#define OPT_MAX_FPS                1012
#define OPT_LOCK_VIDEO_ORIENTATION 1013
#define OPT_DISPLAY_ID             1014
#define OPT_ROTATION               1015
#define OPT_RENDER_DRIVER          1016
#define OPT_NO_MIPMAPS             1017
#define OPT_CODEC_OPTIONS          1018
#define OPT_FORCE_ADB_FORWARD      1019

bool
scrcpy_parse_args(struct scrcpy_cli_args *args, int argc, char *argv[]) {
    static const struct option long_options[] = {
        {"always-on-top",          no_argument,       NULL, OPT_ALWAYS_ON_TOP},
        {"bit-rate",               required_argument, NULL, 'b'},
        {"codec-options",          required_argument, NULL, OPT_CODEC_OPTIONS},
        {"crop",                   required_argument, NULL, OPT_CROP},
        {"display",                required_argument, NULL, OPT_DISPLAY_ID},
        {"force-adb-forward",      no_argument,       NULL,
                                                  OPT_FORCE_ADB_FORWARD},
        {"fullscreen",             no_argument,       NULL, 'f'},
        {"help",                   no_argument,       NULL, 'h'},
        {"lock-video-orientation", required_argument, NULL,
                                                  OPT_LOCK_VIDEO_ORIENTATION},
        {"max-fps",                required_argument, NULL, OPT_MAX_FPS},
        {"max-size",               required_argument, NULL, 'm'},
        {"no-control",             no_argument,       NULL, 'n'},
        {"no-display",             no_argument,       NULL, 'N'},
        {"no-mipmaps",             no_argument,       NULL, OPT_NO_MIPMAPS},
        {"port",                   required_argument, NULL, 'p'},
        {"prefer-text",            no_argument,       NULL, OPT_PREFER_TEXT},
        {"push-target",            required_argument, NULL, OPT_PUSH_TARGET},
        {"record",                 required_argument, NULL, 'r'},
        {"record-format",          required_argument, NULL, OPT_RECORD_FORMAT},
        {"render-driver",          required_argument, NULL, OPT_RENDER_DRIVER},
        {"render-expired-frames",  no_argument,       NULL,
                                                  OPT_RENDER_EXPIRED_FRAMES},
        {"rotation",               required_argument, NULL, OPT_ROTATION},
        {"serial",                 required_argument, NULL, 's'},
        {"show-touches",           no_argument,       NULL, 't'},
        {"stay-awake",             no_argument,       NULL, 'w'},
        {"turn-screen-off",        no_argument,       NULL, 'S'},
        {"verbosity",              required_argument, NULL, 'V'},
        {"version",                no_argument,       NULL, 'v'},
        {"window-title",           required_argument, NULL, OPT_WINDOW_TITLE},
        {"window-x",               required_argument, NULL, OPT_WINDOW_X},
        {"window-y",               required_argument, NULL, OPT_WINDOW_Y},
        {"window-width",           required_argument, NULL, OPT_WINDOW_WIDTH},
        {"window-height",          required_argument, NULL, OPT_WINDOW_HEIGHT},
        {"window-borderless",      no_argument,       NULL,
                                                  OPT_WINDOW_BORDERLESS},
        {NULL,                     0,                 NULL, 0  },
    };

    struct scrcpy_options *opts = &args->opts;

    optind = 0; // reset to start from the first argument in tests

    int c;
    while ((c = getopt_long(argc, argv, "b:c:fF:hm:nNp:r:s:StTvV:w",
                            long_options, NULL)) != -1) {
        switch (c) {
            case 'b':
                if (!parse_bit_rate(optarg, &opts->bit_rate)) {
                    return false;
                }
                break;
            case 'c':
                LOGW("Deprecated option -c. Use --crop instead.");
                // fall through
            case OPT_CROP:
                opts->crop = optarg;
                break;
            case OPT_DISPLAY_ID:
                if (!parse_display_id(optarg, &opts->display_id)) {
                    return false;
                }
                break;
            case 'f':
                opts->fullscreen = true;
                break;
            case 'F':
                LOGW("Deprecated option -F. Use --record-format instead.");
                // fall through
            case OPT_RECORD_FORMAT:
                if (!parse_record_format(optarg, &opts->record_format)) {
                    return false;
                }
                break;
            case 'h':
                args->help = true;
                break;
            case OPT_MAX_FPS:
                if (!parse_max_fps(optarg, &opts->max_fps)) {
                    return false;
                }
                break;
            case 'm':
                if (!parse_max_size(optarg, &opts->max_size)) {
                    return false;
                }
                break;
            case OPT_LOCK_VIDEO_ORIENTATION:
                if (!parse_lock_video_orientation(optarg, &opts->lock_video_orientation)) {
                    return false;
                }
                break;
            case 'n':
                opts->control = false;
                break;
            case 'N':
                opts->display = false;
                break;
            case 'p':
                if (!parse_port_range(optarg, &opts->port_range)) {
                    return false;
                }
                break;
            case 'r':
                opts->record_filename = optarg;
                break;
            case 's':
                opts->serial = optarg;
                break;
            case 'S':
                opts->turn_screen_off = true;
                break;
            case 't':
                opts->show_touches = true;
                break;
            case 'T':
                LOGW("Deprecated option -T. Use --always-on-top instead.");
                // fall through
            case OPT_ALWAYS_ON_TOP:
                opts->always_on_top = true;
                break;
            case 'v':
                args->version = true;
                break;
            case 'V':
                if (!parse_log_level(optarg, &opts->log_level)) {
                    return false;
                }
                break;
            case 'w':
                opts->stay_awake = true;
                break;
            case OPT_RENDER_EXPIRED_FRAMES:
                opts->render_expired_frames = true;
                break;
            case OPT_WINDOW_TITLE:
                opts->window_title = optarg;
                break;
            case OPT_WINDOW_X:
                if (!parse_window_position(optarg, &opts->window_x)) {
                    return false;
                }
                break;
            case OPT_WINDOW_Y:
                if (!parse_window_position(optarg, &opts->window_y)) {
                    return false;
                }
                break;
            case OPT_WINDOW_WIDTH:
                if (!parse_window_dimension(optarg, &opts->window_width)) {
                    return false;
                }
                break;
            case OPT_WINDOW_HEIGHT:
                if (!parse_window_dimension(optarg, &opts->window_height)) {
                    return false;
                }
                break;
            case OPT_WINDOW_BORDERLESS:
                opts->window_borderless = true;
                break;
            case OPT_PUSH_TARGET:
                opts->push_target = optarg;
                break;
            case OPT_PREFER_TEXT:
                opts->prefer_text = true;
                break;
            case OPT_ROTATION:
                if (!parse_rotation(optarg, &opts->rotation)) {
                    return false;
                }
                break;
            case OPT_RENDER_DRIVER:
                opts->render_driver = optarg;
                break;
            case OPT_NO_MIPMAPS:
                opts->mipmaps = false;
                break;
            case OPT_CODEC_OPTIONS:
                opts->codec_options = optarg;
                break;
            case OPT_FORCE_ADB_FORWARD:
                opts->force_adb_forward = true;
                break;
            default:
                // getopt prints the error message on stderr
                return false;
        }
    }

    if (!opts->display && !opts->record_filename) {
        LOGE("-N/--no-display requires screen recording (-r/--record)");
        return false;
    }

    int index = optind;
    if (index < argc) {
        LOGE("Unexpected additional argument: %s", argv[index]);
        return false;
    }

    if (opts->record_format && !opts->record_filename) {
        LOGE("Record format specified without recording");
        return false;
    }

    if (opts->record_filename && !opts->record_format) {
        opts->record_format = guess_record_format(opts->record_filename);
        if (!opts->record_format) {
            LOGE("No format specified for \"%s\" (try with -F mkv)",
                 opts->record_filename);
            return false;
        }
    }

    if (!opts->control && opts->turn_screen_off) {
        LOGE("Could not request to turn screen off if control is disabled");
        return false;
    }

    if (!opts->control && opts->stay_awake) {
        LOGE("Could not request to stay awake if control is disabled");
        return false;
    }

    return true;
}
