#include "common.h"

#include <stdbool.h>
#include <stdio.h>
#ifdef HAVE_V4L2
# include <libavdevice/avdevice.h>
#endif
#define SDL_MAIN_HANDLED // avoid link error on Linux Windows Subsystem
#include <SDL2/SDL.h>

#include "adb/adb.h"
#include "cli.h"
#include "options.h"
#include "scrcpy.h"
#include "usb/scrcpy_otg.h"
#include "util/file.h"
#include "util/log.h"
#include "util/net.h"
#include "util/process.h"
#include "util/thread.h"
#include "version.h"

#ifdef _WIN32
#include <windows.h>
#include "util/str.h"
#endif

// True if the user explicitly targeted a single device (so we must not fan out
// to every connected device).
static bool
has_device_selector(const struct scrcpy_options *opts) {
    return opts->serial
        || opts->select_usb
        || opts->select_tcpip
        || opts->tcpip_dst
        || getenv("ANDROID_SERIAL");
}

// Horizontal step between successive device windows (points). Wider than a
// typical phone mirror so two windows sit side by side instead of stacking.
#define SC_FANOUT_WINDOW_STEP_X 320
// Small vertical stagger so even wide windows (tablets) remain distinguishable.
#define SC_FANOUT_WINDOW_STEP_Y 40
#define SC_FANOUT_WINDOW_BASE_X 40
#define SC_FANOUT_WINDOW_BASE_Y 40

// Spawn one child scrcpy process per serial, each re-running this executable
// with the user's original arguments plus "-s <serial>", then wait for them
// all. Returns SCRCPY_EXIT_SUCCESS only if every child exited successfully.
//
// If `auto_pos` is true (the user did not request a window position), each child
// is given a distinct "--window-x"/"--window-y" so the windows tile instead of
// stacking exactly on top of each other.
static enum scrcpy_exit_code
mirror_all_devices(int argc, char *argv[], char **serials, size_t count,
                   bool auto_pos) {
    char *exe = sc_file_get_executable_path();
    // Fall back to argv[0] if the executable path could not be resolved
    const char *self = exe ? exe : argv[0];

    // child argv = self + user args (argv[1..argc-1]) + "-s" + serial
    //              [+ "--window-x" X "--window-y" Y] + NULL
    size_t extra = auto_pos ? 6 : 2; // "-s" serial [+ 4 window-pos args]
    size_t child_argc = (size_t) argc + extra;
    const char **child_argv = malloc((child_argc + 1) * sizeof(*child_argv));
    if (!child_argv) {
        LOG_OOM();
        free(exe);
        return SCRCPY_EXIT_FAILURE;
    }

    child_argv[0] = self;
    for (int i = 1; i < argc; ++i) {
        child_argv[i] = argv[i];
    }
    child_argv[argc] = "-s";
    // child_argv[argc + 1] (serial) is filled in per device below
    // Window position args, if any, are filled in per device below
    child_argv[child_argc] = NULL;

    sc_pid *pids = malloc(count * sizeof(*pids));
    if (!pids) {
        LOG_OOM();
        free(child_argv);
        free(exe);
        return SCRCPY_EXIT_FAILURE;
    }

    LOGI("Mirroring %" SC_PRIsizet " devices", count);

    // Buffers for the per-device window position strings (reused each spawn;
    // sc_process_execute copies argv before returning)
    char xbuf[16];
    char ybuf[16];

    size_t spawned = 0;
    for (size_t i = 0; i < count; ++i) {
        child_argv[argc + 1] = serials[i];
        if (auto_pos) {
            int x = SC_FANOUT_WINDOW_BASE_X + (int) i * SC_FANOUT_WINDOW_STEP_X;
            int y = SC_FANOUT_WINDOW_BASE_Y + (int) i * SC_FANOUT_WINDOW_STEP_Y;
            snprintf(xbuf, sizeof(xbuf), "%d", x);
            snprintf(ybuf, sizeof(ybuf), "%d", y);
            child_argv[argc + 2] = "--window-x";
            child_argv[argc + 3] = xbuf;
            child_argv[argc + 4] = "--window-y";
            child_argv[argc + 5] = ybuf;
        }
        sc_pid pid;
        enum sc_process_result r =
            sc_process_execute(child_argv, &pid, 0);
        if (r != SC_PROCESS_SUCCESS) {
            LOGE("Could not start scrcpy for device %s", serials[i]);
            continue;
        }
        LOGI("Started scrcpy for device %s", serials[i]);
        pids[spawned++] = pid;
    }

    enum scrcpy_exit_code ret =
        spawned == count ? SCRCPY_EXIT_SUCCESS : SCRCPY_EXIT_FAILURE;
    for (size_t i = 0; i < spawned; ++i) {
        sc_exit_code code = sc_process_wait(pids[i], true);
        if (code != 0) {
            ret = SCRCPY_EXIT_FAILURE;
        }
    }

    free(pids);
    free(child_argv);
    free(exe);
    return ret;
}

// If the user did not target a specific device and several devices are ready,
// mirror them all (one window per device) and write the aggregated exit code to
// *ret. Return true if the fan-out path was taken, false to fall through to the
// normal single-device path.
static bool
try_mirror_all_devices(int argc, char *argv[], const struct scrcpy_options *opts,
                       enum scrcpy_exit_code *ret) {
    if (has_device_selector(opts)) {
        return false;
    }

    if (!sc_adb_init()) {
        return false;
    }

    size_t count;
    char **serials = sc_adb_list_ready_serials(&count);
    if (!serials) {
        // Could not enumerate; let the normal path report the error
        return false;
    }

    bool fan_out = count >= 2;
    if (fan_out) {
        // Only auto-position windows if the user did not request a position
        bool auto_pos = opts->window_x == SC_WINDOW_POSITION_UNDEFINED
                     && opts->window_y == SC_WINDOW_POSITION_UNDEFINED;
        *ret = mirror_all_devices(argc, argv, serials, count, auto_pos);
    }

    for (size_t i = 0; i < count; ++i) {
        free(serials[i]);
    }
    free(serials);

    return fan_out;
}

static int
main_scrcpy(int argc, char *argv[]) {
#ifdef _WIN32
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
        .pause_on_exit = SC_PAUSE_ON_EXIT_FALSE,
    };

#ifndef NDEBUG
    args.opts.log_level = SC_LOG_LEVEL_DEBUG;
#endif

    enum scrcpy_exit_code ret;

    if (!scrcpy_parse_args(&args, argc, argv)) {
        ret = SCRCPY_EXIT_FAILURE;
        goto end;
    }

    sc_set_log_level(args.opts.log_level);

    if (args.help) {
        scrcpy_print_usage(argv[0]);
        ret = SCRCPY_EXIT_SUCCESS;
        goto end;
    }

    if (args.version) {
        scrcpy_print_version();
        ret = SCRCPY_EXIT_SUCCESS;
        goto end;
    }

    // The current thread is the main thread
    SC_MAIN_THREAD_ID = sc_thread_get_id();

#ifdef SCRCPY_LAVF_REQUIRES_REGISTER_ALL
    av_register_all();
#endif

#ifdef HAVE_V4L2
    if (args.opts.v4l2_device) {
        avdevice_register_all();
    }
#endif

    if (!net_init()) {
        ret = SCRCPY_EXIT_FAILURE;
        goto end;
    }

    sc_log_configure();

#ifdef HAVE_USB
    bool otg = args.opts.otg;
#else
    bool otg = false;
#endif

    // In normal mirroring mode, if no specific device was requested and several
    // devices are connected, mirror them all (one window per device) instead of
    // failing with "multiple devices".
    if (!otg && try_mirror_all_devices(argc, argv, &args.opts, &ret)) {
        goto end;
    }

#ifdef HAVE_USB
    ret = otg ? scrcpy_otg(&args.opts) : scrcpy(&args.opts);
#else
    ret = scrcpy(&args.opts);
#endif

end:
    if (args.pause_on_exit == SC_PAUSE_ON_EXIT_TRUE ||
            (args.pause_on_exit == SC_PAUSE_ON_EXIT_IF_ERROR &&
                ret != SCRCPY_EXIT_SUCCESS)) {
        printf("Press Enter to continue...\n");
        getchar();
    }

    return ret;
}

int
main(int argc, char *argv[]) {
#ifndef _WIN32
    return main_scrcpy(argc, argv);
#else
    (void) argc;
    (void) argv;
    int wargc;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) {
        LOG_OOM();
        return SCRCPY_EXIT_FAILURE;
    }

    char **argv_utf8 = malloc((wargc + 1) * sizeof(*argv_utf8));
    if (!argv_utf8) {
        LOG_OOM();
        LocalFree(wargv);
        return SCRCPY_EXIT_FAILURE;
    }

    argv_utf8[wargc] = NULL;

    for (int i = 0; i < wargc; ++i) {
        argv_utf8[i] = sc_str_from_wchars(wargv[i]);
        if (!argv_utf8[i]) {
            LOG_OOM();
            for (int j = 0; j < i; ++j) {
                free(argv_utf8[j]);
            }
            LocalFree(wargv);
            free(argv_utf8);
            return SCRCPY_EXIT_FAILURE;
        }
    }

    LocalFree(wargv);

    int ret = main_scrcpy(wargc, argv_utf8);

    for (int i = 0; i < wargc; ++i) {
        free(argv_utf8[i]);
    }
    free(argv_utf8);

    return ret;
#endif
}
