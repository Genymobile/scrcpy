#include "scrcpy.h"

#include <stdbool.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#define SDL_MAIN_HANDLED // avoid link error on Linux Windows Subsystem
#include <SDL2/SDL.h>

#include "config.h"
#include "cli.h"
#include "compat.h"
#include "util/log.h"

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

int
main(int argc, char *argv[]) {
#ifdef __WINDOWS__
    // disable buffering, we want logs immediately
    // even line buffering (setvbuf() with mode _IOLBF) is not sufficient
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

#ifndef NDEBUG
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

    struct scrcpy_cli_args args = {
        .opts = SCRCPY_OPTIONS_DEFAULT,
        .help = false,
        .version = false,
    };

    if (!scrcpy_parse_args(&args, argc, argv)) {
        return 1;
    }

    if (args.help) {
        scrcpy_print_usage(argv[0]);
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
