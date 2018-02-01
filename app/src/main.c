#include "scrcpy.h"

#include <getopt.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL.h>

#define DEFAULT_LOCAL_PORT 27183
#define DEFAULT_MAX_SIZE 0

struct args {
    const char *serial;
    SDL_bool help;
    Uint16 port;
    Uint16 max_size;
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
        "Shortcuts:\n"
        "\n"
        "    Ctrl+f: switch fullscreen mode\n"
        "    Ctrl+g: resize window to 1:1 (pixel-perfect)\n"
        "    Ctrl+x: resize window to optimal size (remove black borders)\n"
        "\n",
        arg0,
        DEFAULT_MAX_SIZE, DEFAULT_MAX_SIZE ? "" : " (unlimited)",
        DEFAULT_LOCAL_PORT);
}

static int parse_args(struct args *args, int argc, char *argv[]) {
    static const struct option long_options[] = {
        {"help",     no_argument,       NULL, 'h'},
        {"port",     required_argument, NULL, 'p'},
        {"max-size", required_argument, NULL, 'm'},
        {NULL,       0,                 NULL, 0  },
    };
    int c;
    while ((c = getopt_long(argc, argv, "hp:m:", long_options, NULL)) != -1) {
        switch (c) {
            case 'h': {
                args->help = SDL_TRUE;
                break;
            }
            case 'p': {
                char *endptr;
                long value = strtol(optarg, &endptr, 0);
                if (*optarg == '\0' || *endptr != '\0') {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid port: %s\n", optarg);
                    return -1;
                }
                if (value & ~0xffff) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Port out of range: %ld\n", value);
                    return -1;
                }
                args->port = (Uint16) value;
                break;
            }
            case 'm': {
                char *endptr;
                long value = strtol(optarg, &endptr, 0);
                if (*optarg == '\0' || *endptr != '\0') {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid max size: %s\n", optarg);
                    return -1;
                }
                if (value & ~0xffff) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Max size must be between 0 and 65535: %ld\n", value);
                    return -1;
                }
                args->max_size = (Uint16) value;
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
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unexpected additional argument: %s\n", argv[index]);
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int res;

    struct args args = {
        .help = SDL_FALSE,
        .serial = NULL,
        .max_size = DEFAULT_MAX_SIZE,
        .port = DEFAULT_LOCAL_PORT,
    };
    if (parse_args(&args, argc, argv)) {
        usage(argv[0]);
        return 1;
    }

    if (args.help) {
        usage(argv[0]);
        return 0;
    }

    av_register_all();

    if (avformat_network_init()) {
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    res = scrcpy(args.serial, args.port, args.max_size) ? 0 : 1;

    avformat_network_deinit(); // ignore failure

    return res;
}
