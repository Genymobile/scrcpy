#include "common.h"

#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#ifdef HAVE_V4L2
# include <libavdevice/avdevice.h>
#endif
#define SDL_MAIN_HANDLED // avoid link error on Linux Windows Subsystem
#include <SDL2/SDL.h>

#include "cli.h"
#include "options.h"
#include "scrcpy.h"
#include "usb/scrcpy_otg.h"
#include "util/log.h"
#include "version.h"

int
main(int argc, char *argv[]) {
#ifdef __WINDOWS__
    // disable buffering, we want logs immediately
    // even line buffering (setvbuf() with mode _IOLBF) is not sufficient
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
#endif

    printf("scrcpy " SCRCPY_VERSION
           " <https://github.com/Genymobile/scrcpy>\n");

    struct scrcpy_cli_args args = {
        .opts = scrcpy_options_default,
        .help = false,
        .version = false,
    };

#ifndef NDEBUG
    args.opts.log_level = SC_LOG_LEVEL_DEBUG;
#endif

    if (!scrcpy_parse_args(&args, argc, argv)) {
        return 1;
    }

    sc_set_log_level(args.opts.log_level);

    if (args.help) {
        scrcpy_print_usage(argv[0]);
        return 0;
    }

    if (args.version) {
        scrcpy_print_version();
        return 0;
    }

#ifdef SCRCPY_LAVF_REQUIRES_REGISTER_ALL
    av_register_all();
#endif

#ifdef HAVE_V4L2
    if (args.opts.v4l2_device) {
        avdevice_register_all();
    }
#endif

    if (avformat_network_init()) {
        return 1;
    }

#ifdef HAVE_USB
    bool ok = args.opts.otg ? scrcpy_otg(&args.opts)
                            : scrcpy(&args.opts);
#else
    bool ok = scrcpy(&args.opts);
#endif

    avformat_network_deinit(); // ignore failure

    return ok ? 0 : 1;
}
