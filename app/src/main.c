#include "scrcpy.h"

#include <getopt.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL.h>

#include "config.h"

struct args {
    const char *serial;
    SDL_bool help;
    SDL_bool version;
    Uint16 port;
    Uint16 max_size;
    Uint32 bit_rate;
};

static void usage(const char *arg0) {
    fprintf(stderr,
        "Usage: %s [options] [serial]\n"
        "\n"
        "    serial\n"
        "        The device serial number. Mandatory only if several devices\n"
        "        are connected to adb.\n"
        "\n"
        "Options:\n"
        "\n"
        "    -b, --bit-rate value\n"
        "        Encode the video at the given bit-rate, expressed in bits/s.\n"
        "        Unit suffixes are supported: 'K' (x1000) and 'M' (x1000000).\n"
        "        Default is %d.\n"
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
        "        resize window to optimal size (remove black borders)\n"
        "\n"
        "    Ctrl+h\n"
        "    Home\n"
        "        click on HOME\n"
        "\n"
        "    Ctrl+b\n"
        "    Ctrl+Backspace\n"
        "        click on BACK\n"
        "\n"
        "    Ctrl+m\n"
        "        click on APP_SWITCH\n"
        "\n"
        "    Ctrl+'+'\n"
        "        click on VOLUME_UP\n"
        "\n"
        "    Ctrl+'-'\n"
        "        click on VOLUME_DOWN\n"
        "\n"
        "    Ctrl+p\n"
        "        click on POWER (turn screen on/off)\n"
        "\n"
        "    Right-click\n"
        "        turn screen on\n"
        "\n",
        arg0,
        DEFAULT_BIT_RATE,
        DEFAULT_MAX_SIZE, DEFAULT_MAX_SIZE ? "" : " (unlimited)",
        DEFAULT_LOCAL_PORT);
}

static int parse_args(struct args *args, int argc, char *argv[]) {
    static const struct option long_options[] = {
        {"help",     no_argument,       NULL, 'h'},
        {"port",     required_argument, NULL, 'p'},
        {"max-size", required_argument, NULL, 'm'},
        {"bit-rate", required_argument, NULL, 'b'},
        {"version",  no_argument,       NULL, 'v'},
        {NULL,       0,                 NULL, 0  },
    };
    int c;
    while ((c = getopt_long(argc, argv, "hvp:m:b:", long_options, NULL)) != -1) {
        switch (c) {
            case 'h': {
                args->help = SDL_TRUE;
                break;
            }
            case 'v': {
                args->version = SDL_TRUE;
                break;
            }
            case 'p': {
                char *endptr;
                if (*optarg == '\0') {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid port parameter is empty");
                    return -1;
                }
                long value = strtol(optarg, &endptr, 0);
                if (*endptr != '\0') {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid port: %s", optarg);
                    return -1;
                }
                if (value & ~0xffff) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Port out of range: %ld", value);
                    return -1;
                }
                args->port = (Uint16) value;
                break;
            }
            case 'm': {
                char *endptr;
                if (*optarg == '\0') {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Max size parameter is empty");
                    return -1;
                }
                long value = strtol(optarg, &endptr, 0);
                if (*endptr != '\0') {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid max size: %s", optarg);
                    return -1;
                }
                if (value & ~0xffff) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Max size must be between 0 and 65535: %ld", value);
                    return -1;
                }
                args->max_size = (Uint16) value;
                break;
            }
            case 'b': {
                char *endptr;
                if (*optarg == '\0') {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Bit-rate parameter is empty");
                    return -1;
                }
                long value = strtol(optarg, &endptr, 0);
                int mul = 1;
                if (*endptr != '\0') {
                    if (optarg == endptr) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid bit-rate: %s", optarg);
                        return -1;
                    }
                    if ((*endptr == 'M' || *endptr == 'm') && endptr[1] == '\0') {
                        mul = 1000000;
                    } else if ((*endptr == 'K' || *endptr == 'k') && endptr[1] == '\0') {
                        mul = 1000;
                    } else {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid bit-rate unit: %s", optarg);
                        return -1;
                    }
                }
                if (value < 0 || ((Uint32) -1) / mul < value) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Bitrate must be positive and less than 2^32: %s", optarg);
                    return -1;
                }
                args->bit_rate = (Uint32) value * mul;
                break;
            }
            default:
                // getopt prints the error message on stderr
                return -1;
        }
    }

    int index = optind;
    if (index < argc) {
        args->serial = argv[index++];
    }
    if (index < argc) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unexpected additional argument: %s", argv[index]);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    struct args args = {
        .serial = NULL,
        .help = SDL_FALSE,
        .version = SDL_FALSE,
        .port = DEFAULT_LOCAL_PORT,
        .max_size = DEFAULT_MAX_SIZE,
        .bit_rate = DEFAULT_BIT_RATE,
    };
    if (parse_args(&args, argc, argv)) {
        usage(argv[0]);
        return 1;
    }

    if (args.help) {
        usage(argv[0]);
        return 0;
    }

    if (args.version) {
        fprintf(stderr, "scrcpy v%s\n", SCRCPY_VERSION);
        return 0;
    }

    av_register_all();

    if (avformat_network_init()) {
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    int res = scrcpy(args.serial, args.port, args.max_size, args.bit_rate) ? 0 : 1;

    avformat_network_deinit(); // ignore failure

    return res;
}
