#include "scrcpy.h"

#include <getopt.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL.h>

#include "config.h"
#include "log.h"

struct args {
    const char *serial;
    const char *crop;
    SDL_bool help;
    SDL_bool version;
    SDL_bool show_touches;
    Uint16 port;
    Uint16 max_size;
    Uint32 bit_rate;
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
        "    -h, --help\n"
        "        Print this help.\n"
        "\n"
        "    -m, --max-size value\n"
        "        Limit both the width and height of the video to value. The\n"
        "        other dimension is computed so that the device aspect-ratio\n"
        "        is preserved.\n"
        "        Default is %d%s.\n"
        "\n"
        "    -p, --port port\n"
        "        Set the TCP port the client listens on.\n"
        "        Default is %d.\n"
        "\n"
        "    -s, --serial\n"
        "        The device serial number. Mandatory only if several devices\n"
        "        are connected to adb.\n"
        "\n"
        "    -t, --show-touches\n"
        "        Enable \"show touches\" on start, disable on quit.\n"
        "        It only shows physical touches (not clicks from scrcpy).\n"
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

static void print_version(void) {
    fprintf(stderr, "scrcpy v%s\n\n", SCRCPY_VERSION);

    fprintf(stderr, "dependencies:\n");
    fprintf(stderr, " - SDL %d.%d.%d\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    fprintf(stderr, " - libavcodec %d.%d.%d\n", LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
    fprintf(stderr, " - libavformat %d.%d.%d\n", LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    fprintf(stderr, " - libavutil %d.%d.%d\n", LIBAVUTIL_VERSION_MAJOR, LIBAVUTIL_VERSION_MINOR, LIBAVUTIL_VERSION_MICRO);
}

static SDL_bool parse_bit_rate(char *optarg, Uint32 *bit_rate) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Bit-rate parameter is empty");
        return SDL_FALSE;
    }
    long value = strtol(optarg, &endptr, 0);
    int mul = 1;
    if (*endptr != '\0') {
        if (optarg == endptr) {
            LOGE("Invalid bit-rate: %s", optarg);
            return SDL_FALSE;
        }
        if ((*endptr == 'M' || *endptr == 'm') && endptr[1] == '\0') {
            mul = 1000000;
        } else if ((*endptr == 'K' || *endptr == 'k') && endptr[1] == '\0') {
            mul = 1000;
        } else {
            LOGE("Invalid bit-rate unit: %s", optarg);
            return SDL_FALSE;
        }
    }
    if (value < 0 || ((Uint32) -1) / mul < value) {
        LOGE("Bitrate must be positive and less than 2^32: %s", optarg);
        return SDL_FALSE;
    }

    *bit_rate = (Uint32) value * mul;
    return SDL_TRUE;
}

static SDL_bool parse_max_size(char *optarg, Uint16 *max_size) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Max size parameter is empty");
        return SDL_FALSE;
    }
    long value = strtol(optarg, &endptr, 0);
    if (*endptr != '\0') {
        LOGE("Invalid max size: %s", optarg);
        return SDL_FALSE;
    }
    if (value & ~0xffff) {
        LOGE("Max size must be between 0 and 65535: %ld", value);
        return SDL_FALSE;
    }

    *max_size = (Uint16) value;
    return SDL_TRUE;
}

static SDL_bool parse_port(char *optarg, Uint16 *port) {
    char *endptr;
    if (*optarg == '\0') {
        LOGE("Invalid port parameter is empty");
        return SDL_FALSE;
    }
    long value = strtol(optarg, &endptr, 0);
    if (*endptr != '\0') {
        LOGE("Invalid port: %s", optarg);
        return SDL_FALSE;
    }
    if (value & ~0xffff) {
        LOGE("Port out of range: %ld", value);
        return SDL_FALSE;
    }

    *port = (Uint16) value;
    return SDL_TRUE;
}

static SDL_bool parse_args(struct args *args, int argc, char *argv[]) {
    static const struct option long_options[] = {
        {"bit-rate",     required_argument, NULL, 'b'},
        {"crop",         required_argument, NULL, 'c'},
        {"help",         no_argument,       NULL, 'h'},
        {"max-size",     required_argument, NULL, 'm'},
        {"port",         required_argument, NULL, 'p'},
        {"serial",       required_argument, NULL, 's'},
        {"show-touches", no_argument,       NULL, 't'},
        {"version",      no_argument,       NULL, 'v'},
        {NULL,           0,                 NULL, 0  },
    };
    int c;
    while ((c = getopt_long(argc, argv, "b:c:hm:p:s:tv", long_options, NULL)) != -1) {
        switch (c) {
            case 'b':
                if (!parse_bit_rate(optarg, &args->bit_rate)) {
                    return SDL_FALSE;
                }
                break;
            case 'c':
                args->crop = optarg;
                break;
            case 'h':
                args->help = SDL_TRUE;
                break;
            case 'm':
                if (!parse_max_size(optarg, &args->max_size)) {
                    return SDL_FALSE;
                }
                break;
            case 'p':
                if (!parse_port(optarg, &args->port)) {
                    return SDL_FALSE;
                }
                break;
            case 's':
                args->serial = optarg;
                break;
            case 't':
                args->show_touches = SDL_TRUE;
                break;
            case 'v':
                args->version = SDL_TRUE;
                break;
            default:
                // getopt prints the error message on stderr
                return SDL_FALSE;
        }
    }

    int index = optind;
    if (index < argc) {
        LOGE("Unexpected additional argument: %s", argv[index]);
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

int main(int argc, char *argv[]) {
#ifdef __WINDOWS__
    // disable buffering, we want logs immediately
    // even line buffering (setvbuf() with mode _IOLBF) is not sufficient
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif
    struct args args = {
        .serial = NULL,
        .crop = NULL,
        .help = SDL_FALSE,
        .version = SDL_FALSE,
        .show_touches = SDL_FALSE,
        .port = DEFAULT_LOCAL_PORT,
        .max_size = DEFAULT_MAX_SIZE,
        .bit_rate = DEFAULT_BIT_RATE,
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

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
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
        .max_size = args.max_size,
        .bit_rate = args.bit_rate,
        .show_touches = args.show_touches,
    };
    int res = scrcpy(&options) ? 0 : 1;

    avformat_network_deinit(); // ignore failure

    return res;
}
