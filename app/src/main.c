#include "scrcpy.h"

#include <unistd.h>
#include <libavformat/avformat.h>
#include <SDL2/SDL.h>

#define DEFAULT_LOCAL_PORT 27183

struct args {
    const char *serial;
    Uint16 port;
    Uint16 maximum_size;
};

int parse_args(struct args *args, int argc, char *argv[]) {
    int c;
    while ((c = getopt(argc, argv, "p:m:")) != -1) {
        switch (c) {
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
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid maximum size: %s\n", optarg);
                return -1;
            }
            if (value & ~0xffff) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Maximum size must be between 0 and 65535: %ld\n", value);
                return -1;
            }
            args->maximum_size = (Uint16) value;
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
        .serial = NULL,
        .port = DEFAULT_LOCAL_PORT,
    };
    if (parse_args(&args, argc, argv)) {
        return 1;
    }

    av_register_all();

    if (avformat_network_init()) {
        return 1;
    }

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    res = scrcpy(args.serial, args.port, args.maximum_size) ? 0 : 1;

    avformat_network_deinit(); // ignore failure

    return res;
}
