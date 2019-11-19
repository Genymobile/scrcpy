#include "scrcpy.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#define SDL_MAIN_HANDLED // avoid link error on Linux Windows Subsystem
#include <SDL2/SDL.h>

#include "config.h"
#include "compat.h"
#include "log.h"
#include "recorder.h"

struct args {
    struct scrcpy_options opts;
    bool help;
    bool version;
};

static void usage(const char *arg0) {
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
        "    --crop width:height:x:y\n"
        "        Crop the device screen on the server.\n"
        "        The values are expressed in the device natural orientation\n"
        "        (typically, portrait for a phone, landscape for a tablet).\n"
        "        Any --max-size value is computed on the cropped size.\n"
        "\n"
        "    -f, --fullscreen\n"
        "        Start in fullscreen.\n"
        "\n"
        "    -h, --help\n"
        "        Print this help.\n"
        "\n"
        "    --max-fps value\n"
        "        Limit the frame rate of screen capture (only supported on\n"
        "        devices with Android >= 10).\n"
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
        "    -p, --port port\n"
        "        Set the TCP port the client listens on.\n"
        "        Default is %d.\n"
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
        "    --render-expired-frames\n"
        "        By default, to minimize latency, scrcpy always renders the\n"
        "        last available decoded frame, and drops any previous ones.\n"
        "        This flag forces to render all frames, at a cost of a\n"
        "        possible increased latency.\n"
        "\n"
        "    -s, --serial serial\n"
        "        The device serial number. Mandatory only if several devices\n"
        "        are connected to adb.\n"
        "\n"
        "    -S, --turn-screen-off\n"
        "        Turn the device screen off immediately.\n"
        "\n"
        "    -t, --show-touches\n"
        "        Enable \"show touches\" on start, disable on quit.\n"
        "        It only shows physical touches (not clicks from scrcpy).\n"
        "\n"
        "    -v, --version\n"
        "        Print the version of scrcpy.\n"
        "\n"
        "    --window-borderless\n"
        "        Disable window decorations (display borderless window).\n"
        "\n"
        "    --window-title text\n"
        "        Set a custom window title.\n"
        "\n"
        "    --window-x value\n"
        "        Set the initial window horizontal position.\n"
        "        Default is -1 (automatic).\n"
        "\n"
        "    --window-y value\n"
        "        Set the initial window vertical position.\n"
        "        Default is -1 (automatic).\n"
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
        "        switch fullscreen mode\n"
        "\n"
        "    " CTRL_OR_CMD "+g\n"
        "        resize window to 1:1 (pixel-perfect)\n"
        "\n"
        "    " CTRL_OR_CMD "+x\n"
        "    Double-click on black borders\n"
        "        resize window to remove black borders\n"
        "\n"
        "    Ctrl+h\n"
        "    Middle-click\n"
        "        click on HOME\n"
        "\n"
        "    " CTRL_OR_CMD "+b\n"
        "    " CTRL_OR_CMD "+Backspace\n"
        "    Right-click (when screen is on)\n"
        "        click on BACK\n"
        "\n"
        "    " CTRL_OR_CMD "+s\n"
        "        click on APP_SWITCH\n"
        "\n"
        "    Ctrl+m\n"
        "        click on MENU\n"
        "\n"
        "    " CTRL_OR_CMD "+Up\n"
        "        click on VOLUME_UP\n"
        "\n"
        "    " CTRL_OR_CMD "+Down\n"
        "        click on VOLUME_DOWN\n"
        "\n"
        "    " CTRL_OR_CMD "+p\n"
        "        click on POWER (turn screen on/off)\n"
        "\n"
        "    Right-click (when screen is off)\n"
        "        power on\n"
        "\n"
        "    " CTRL_OR_CMD "+o\n"
        "        turn device screen off (keep mirroring)\n"
        "\n"
        "    " CTRL_OR_CMD "+n\n"
        "       expand notification panel\n"
        "\n"
        "    " CTRL_OR_CMD "+Shift+n\n"
        "       collapse notification panel\n"
        "\n"
        "    " CTRL_OR_CMD "+c\n"
        "        copy device clipboard to computer\n"
        "\n"
        "    " CTRL_OR_CMD "+v\n"
        "        paste computer clipboard to device\n"
        "\n"
        "    " CTRL_OR_CMD "+Shift+v\n"
        "        copy computer clipboard to device\n"
        "\n"
        "    " CTRL_OR_CMD "+i\n"
        "        enable/disable FPS counter (print frames/second in logs)\n"
        "\n"
        "    Drag & drop APK file\n"
        "        install APK from computer\n"
        "\n",
        arg0,
        DEFAULT_BIT_RATE,
        DEFAULT_MAX_SIZE, DEFAULT_MAX_SIZE ? "" : " (unlimited)",
        DEFAULT_LOCAL_PORT);
}

static void
print_version(void) {
    fprintf(stderr, "scrcpy %s\n\n", SCRCPY_VERSION);

    fprintf(stderr, "dependencies:\n");
    fprintf(stderr, " - SDL %d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
                                         SDL_PATCHLEVEL);
    fprintf(stderr, " - libavcodec %d.%d.%d\n", LIBAVCODEC_VERSION_MAJOR,
                                                LIBAVCODEC_VERSION_MINOR,
                                                LIBAVCODEC_VERSION_MICRO);
    fprintf(stderr, " - libavformat %d.%d.%d\n", LIBAVFORMAT_VERSION_MAJOR,
                                                 LIBAVFORMAT_VERSION_MINOR,
                                                 LIBAVFORMAT_VERSION_MICRO);
    fprintf(stderr, " - libavutil %d.%d.%d\n", LIBAVUTIL_VERSION_MAJOR,
                                               LIBAVUTIL_VERSION_MINOR,
                                               LIBAVUTIL_VERSION_MICRO);
}

static bool
parse_bit_rate(char *optarg, uint32_t *bit_rate) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Bit-rate parameter is empty");
        return false;
    }
    long value = strtol(optarg, &endptr, 0);
    int mul = 1;
    if (*endptr != '\0') {
        if (optarg == endptr) {
            LOGE("Invalid bit-rate: %s", optarg);
            return false;
        }
        if ((*endptr == 'M' || *endptr == 'm') && endptr[1] == '\0') {
            mul = 1000000;
        } else if ((*endptr == 'K' || *endptr == 'k') && endptr[1] == '\0') {
            mul = 1000;
        } else {
            LOGE("Invalid bit-rate unit: %s", optarg);
            return false;
        }
    }
    if (value < 0 || ((uint32_t) -1) / mul < value) {
        LOGE("Bitrate must be positive and less than 2^32: %s", optarg);
        return false;
    }

    *bit_rate = (uint32_t) value * mul;
    return true;
}

static bool
parse_max_size(char *optarg, uint16_t *max_size) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Max size parameter is empty");
        return false;
    }
    long value = strtol(optarg, &endptr, 0);
    if (*endptr != '\0') {
        LOGE("Invalid max size: %s", optarg);
        return false;
    }
    if (value & ~0xffff) {
        LOGE("Max size must be between 0 and 65535: %ld", value);
        return false;
    }

    *max_size = (uint16_t) value;
    return true;
}

static bool
parse_max_fps(const char *optarg, uint16_t *max_fps) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Max FPS parameter is empty");
        return false;
    }
    long value = strtol(optarg, &endptr, 0);
    if (*endptr != '\0') {
        LOGE("Invalid max FPS: %s", optarg);
        return false;
    }
    if (value & ~0xffff) {
        // in practice, it should not be higher than 60
        LOGE("Max FPS value is invalid: %ld", value);
        return false;
    }

    *max_fps = (uint16_t) value;
    return true;
}

static bool
parse_window_position(char *optarg, int16_t *position) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Window position parameter is empty");
        return false;
    }
    long value = strtol(optarg, &endptr, 0);
    if (*endptr != '\0') {
        LOGE("Invalid window position: %s", optarg);
        return false;
    }
    if (value < -1 || value > 0x7fff) {
        LOGE("Window position must be between -1 and 32767: %ld", value);
        return false;
    }

    *position = (int16_t) value;
    return true;
}

static bool
parse_window_dimension(char *optarg, uint16_t *dimension) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Window dimension parameter is empty");
        return false;
    }
    long value = strtol(optarg, &endptr, 0);
    if (*endptr != '\0') {
        LOGE("Invalid window dimension: %s", optarg);
        return false;
    }
    if (value & ~0xffff) {
        LOGE("Window position must be between 0 and 65535: %ld", value);
        return false;
    }

    *dimension = (uint16_t) value;
    return true;
}

static bool
parse_port(char *optarg, uint16_t *port) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Port parameter is empty");
        return false;
    }
    long value = strtol(optarg, &endptr, 0);
    if (*endptr != '\0') {
        LOGE("Invalid port: %s", optarg);
        return false;
    }
    if (value & ~0xffff) {
        LOGE("Port out of range: %ld", value);
        return false;
    }

    *port = (uint16_t) value;
    return true;
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

#define OPT_RENDER_EXPIRED_FRAMES 1000
#define OPT_WINDOW_TITLE          1001
#define OPT_PUSH_TARGET           1002
#define OPT_ALWAYS_ON_TOP         1003
#define OPT_CROP                  1004
#define OPT_RECORD_FORMAT         1005
#define OPT_PREFER_TEXT           1006
#define OPT_WINDOW_X              1007
#define OPT_WINDOW_Y              1008
#define OPT_WINDOW_WIDTH          1009
#define OPT_WINDOW_HEIGHT         1010
#define OPT_WINDOW_BORDERLESS     1011
#define OPT_MAX_FPS               1012

static bool
parse_args(struct args *args, int argc, char *argv[]) {
    static const struct option long_options[] = {
        {"always-on-top",         no_argument,       NULL, OPT_ALWAYS_ON_TOP},
        {"bit-rate",              required_argument, NULL, 'b'},
        {"crop",                  required_argument, NULL, OPT_CROP},
        {"fullscreen",            no_argument,       NULL, 'f'},
        {"help",                  no_argument,       NULL, 'h'},
        {"max-fps",               required_argument, NULL, OPT_MAX_FPS},
        {"max-size",              required_argument, NULL, 'm'},
        {"no-control",            no_argument,       NULL, 'n'},
        {"no-display",            no_argument,       NULL, 'N'},
        {"port",                  required_argument, NULL, 'p'},
        {"push-target",           required_argument, NULL, OPT_PUSH_TARGET},
        {"record",                required_argument, NULL, 'r'},
        {"record-format",         required_argument, NULL, OPT_RECORD_FORMAT},
        {"render-expired-frames", no_argument,       NULL,
                                                     OPT_RENDER_EXPIRED_FRAMES},
        {"serial",                required_argument, NULL, 's'},
        {"show-touches",          no_argument,       NULL, 't'},
        {"turn-screen-off",       no_argument,       NULL, 'S'},
        {"prefer-text",           no_argument,       NULL, OPT_PREFER_TEXT},
        {"version",               no_argument,       NULL, 'v'},
        {"window-title",          required_argument, NULL, OPT_WINDOW_TITLE},
        {"window-x",              required_argument, NULL, OPT_WINDOW_X},
        {"window-y",              required_argument, NULL, OPT_WINDOW_Y},
        {"window-width",          required_argument, NULL, OPT_WINDOW_WIDTH},
        {"window-height",         required_argument, NULL, OPT_WINDOW_HEIGHT},
        {"window-borderless",     no_argument,       NULL,
                                                     OPT_WINDOW_BORDERLESS},
        {NULL,                    0,                 NULL, 0  },
    };

    struct scrcpy_options *opts = &args->opts;

    int c;
    while ((c = getopt_long(argc, argv, "b:c:fF:hm:nNp:r:s:StTv", long_options,
                            NULL)) != -1) {
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
            case 'n':
                opts->control = false;
                break;
            case 'N':
                opts->display = false;
                break;
            case 'p':
                if (!parse_port(optarg, &opts->port)) {
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
            default:
                // getopt prints the error message on stderr
                return false;
        }
    }

    if (!opts->display && !opts->record_filename) {
        LOGE("-N/--no-display requires screen recording (-r/--record)");
        return false;
    }

    if (!opts->display && opts->fullscreen) {
        LOGE("-f/--fullscreen-window is incompatible with -N/--no-display");
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

    return true;
}

int
main(int argc, char *argv[]) {
#ifdef __WINDOWS__
    // disable buffering, we want logs immediately
    // even line buffering (setvbuf() with mode _IOLBF) is not sufficient
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif
    struct args args = {
        .opts = SCRCPY_OPTIONS_DEFAULT,
        .help = false,
        .version = false,
    };

    if (!parse_args(&args, argc, argv)) {
        return 1;
    }

    if (args.help) {
        usage(argv[0]);
        return 0;
    }

    if (args.version) {
        print_version();
        return 0;
    }

    LOGI("scrcpy " SCRCPY_VERSION " <https://github.com/Genymobile/scrcpy>");

#ifdef SCRCPY_LAVF_REQUIRES_REGISTER_ALL
    av_register_all();
#endif

    if (avformat_network_init()) {
        return 1;
    }

#ifdef BUILD_DEBUG
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

    int res = scrcpy(&args.opts) ? 0 : 1;

    avformat_network_deinit(); // ignore failure

#if defined (__WINDOWS__) && ! defined (WINDOWS_NOCONSOLE)
    if (res != 0) {
        fprintf(stderr, "Press any key to continue...\n");
        getchar();
    }
#endif
    return res;
}
