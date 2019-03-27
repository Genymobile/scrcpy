#include "scrcpy.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#define SDL_MAIN_HANDLED // avoid link error on Linux Windows Subsystem
#include <SDL2/SDL.h>

#include "compat.h"
#include "config.h"
#include "log.h"
#include "recorder.h"

struct args {
    const char *serial;
    const char *crop;
    const char *record_filename;
    enum recorder_format record_format;
    bool fullscreen;
    bool no_control;
    bool no_display;
    bool help;
    bool version;
    bool show_touches;
    uint16_t port;
    uint16_t max_size;
    uint32_t bit_rate;
    bool always_on_top;
};

static void usage(const char *arg0) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "\n"
        "    -b, --bit-rate value\n"
        "        Encode the video at the given bit-rate, expressed in bits/s.\n"
        "        Unit suffixes are supported: 'K' (x1000) and 'M' (x1000000).\n"
        "        Default is %d.\n"
        "\n"
        "    -c, --crop width:height:x:y\n"
        "        Crop the device screen on the server.\n"
        "        The values are expressed in the device natural orientation\n"
        "        (typically, portrait for a phone, landscape for a tablet).\n"
        "        Any --max-size value is computed on the cropped size.\n"
        "\n"
        "    -f, --fullscreen\n"
        "        Start in fullscreen.\n"
        "\n"
        "    -F, --record-format\n"
        "        Force recording format (either mp4 or mkv).\n"
        "\n"
        "    -h, --help\n"
        "        Print this help.\n"
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
        "    -r, --record file.mp4\n"
        "        Record screen to file.\n"
        "        The format is determined by the -F/--record-format option if\n"
        "        set, or by the file extension (.mp4 or .mkv).\n"
        "\n"
        "    -s, --serial\n"
        "        The device serial number. Mandatory only if several devices\n"
        "        are connected to adb.\n"
        "\n"
        "    -t, --show-touches\n"
        "        Enable \"show touches\" on start, disable on quit.\n"
        "        It only shows physical touches (not clicks from scrcpy).\n"
        "\n"
        "    -T, --always-on-top\n"
        "        Make scrcpy window always on top (above other windows).\n"
        "\n"
        "    -v, --version\n"
        "        Print the version of scrcpy.\n"
        "\n"
        "Shortcuts:\n"
        "\n"
        "    Ctrl+f\n"
        "        switch fullscreen mode\n"
        "\n"
        "    Ctrl+g\n"
        "        resize window to 1:1 (pixel-perfect)\n"
        "\n"
        "    Ctrl+x\n"
        "    Double-click on black borders\n"
        "        resize window to remove black borders\n"
        "\n"
        "    Ctrl+h\n"
        "    Home\n"
        "    Middle-click\n"
        "        click on HOME\n"
        "\n"
        "    Ctrl+b\n"
        "    Ctrl+Backspace\n"
        "    Right-click (when screen is on)\n"
        "        click on BACK\n"
        "\n"
        "    Ctrl+s\n"
        "        click on APP_SWITCH\n"
        "\n"
        "    Ctrl+m\n"
        "        click on MENU\n"
        "\n"
        "    Ctrl+Up\n"
        "        click on VOLUME_UP\n"
        "\n"
        "    Ctrl+Down\n"
        "        click on VOLUME_DOWN\n"
        "\n"
        "    Ctrl+p\n"
        "        click on POWER (turn screen on/off)\n"
        "\n"
        "    Right-click (when screen is off)\n"
        "        turn screen on\n"
        "\n"
        "    Ctrl+n\n"
        "       expand notification panel\n"
        "\n"
        "    Ctrl+Shift+n\n"
        "       collapse notification panel\n"
        "\n"
        "    Ctrl+v\n"
        "        paste computer clipboard to device\n"
        "\n"
        "    Ctrl+i\n"
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
parse_port(char *optarg, uint16_t *port) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Invalid port parameter is empty");
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

static bool
parse_args(struct args *args, int argc, char *argv[]) {
    static const struct option long_options[] = {
        {"always-on-top", no_argument,       NULL, 'T'},
        {"bit-rate",      required_argument, NULL, 'b'},
        {"crop",          required_argument, NULL, 'c'},
        {"fullscreen",    no_argument,       NULL, 'f'},
        {"help",          no_argument,       NULL, 'h'},
        {"max-size",      required_argument, NULL, 'm'},
        {"no-control",    no_argument,       NULL, 'n'},
        {"no-display",    no_argument,       NULL, 'N'},
        {"port",          required_argument, NULL, 'p'},
        {"record",        required_argument, NULL, 'r'},
        {"record-format", required_argument, NULL, 'f'},
        {"serial",        required_argument, NULL, 's'},
        {"show-touches",  no_argument,       NULL, 't'},
        {"version",       no_argument,       NULL, 'v'},
        {NULL,            0,                 NULL, 0  },
    };
    int c;
    while ((c = getopt_long(argc, argv, "b:c:fF:hm:nNp:r:s:tTv", long_options,
                            NULL)) != -1) {
        switch (c) {
            case 'b':
                if (!parse_bit_rate(optarg, &args->bit_rate)) {
                    return false;
                }
                break;
            case 'c':
                args->crop = optarg;
                break;
            case 'f':
                args->fullscreen = true;
                break;
            case 'F':
                if (!parse_record_format(optarg, &args->record_format)) {
                    return false;
                }
                break;
            case 'h':
                args->help = true;
                break;
            case 'm':
                if (!parse_max_size(optarg, &args->max_size)) {
                    return false;
                }
                break;
            case 'n':
                args->no_control = true;
                break;
            case 'N':
                args->no_display = true;
                break;
            case 'p':
                if (!parse_port(optarg, &args->port)) {
                    return false;
                }
                break;
            case 'r':
                args->record_filename = optarg;
                break;
            case 's':
                args->serial = optarg;
                break;
            case 't':
                args->show_touches = true;
                break;
            case 'T':
                args->always_on_top = true;
                break;
            case 'v':
                args->version = true;
                break;
            default:
                // getopt prints the error message on stderr
                return false;
        }
    }

    if (args->no_display && !args->record_filename) {
        LOGE("-N/--no-display requires screen recording (-r/--record)");
        return false;
    }

    if (args->no_display && args->fullscreen) {
        LOGE("-f/--fullscreen-window is incompatible with -N/--no-display");
        return false;
    }

    int index = optind;
    if (index < argc) {
        LOGE("Unexpected additional argument: %s", argv[index]);
        return false;
    }

    if (args->record_format && !args->record_filename) {
        LOGE("Record format specified without recording");
        return false;
    }

    if (args->record_filename && !args->record_format) {
        args->record_format = guess_record_format(args->record_filename);
        if (!args->record_format) {
            LOGE("No format specified for \"%s\" (try with -F mkv)",
                 args->record_filename);
            return false;
        }
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
        .serial = NULL,
        .crop = NULL,
        .record_filename = NULL,
        .record_format = 0,
        .help = false,
        .version = false,
        .show_touches = false,
        .port = DEFAULT_LOCAL_PORT,
        .max_size = DEFAULT_MAX_SIZE,
        .bit_rate = DEFAULT_BIT_RATE,
        .always_on_top = false,
        .no_control = false,
        .no_display = false,
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

#ifdef SCRCPY_LAVF_REQUIRES_REGISTER_ALL
    av_register_all();
#endif

    if (avformat_network_init()) {
        return 1;
    }

#ifdef BUILD_DEBUG
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

    struct scrcpy_options options = {
        .serial = args.serial,
        .crop = args.crop,
        .port = args.port,
        .record_filename = args.record_filename,
        .record_format = args.record_format,
        .max_size = args.max_size,
        .bit_rate = args.bit_rate,
        .show_touches = args.show_touches,
        .fullscreen = args.fullscreen,
        .always_on_top = args.always_on_top,
        .no_control = args.no_control,
        .no_display = args.no_display,
    };
    int res = scrcpy(&options) ? 0 : 1;

    avformat_network_deinit(); // ignore failure

#if defined (__WINDOWS__) && ! defined (WINDOWS_NOCONSOLE)
    if (res != 0) {
        fprintf(stderr, "Press any key to continue...\n");
        getchar();
    }
#endif
    return res;
}
