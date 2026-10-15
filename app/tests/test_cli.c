#include "common.h"

#include <assert.h>
#include <string.h>

#include "cli.h"
#include "options.h"

static void test_flag_version(void) {
    struct scrcpy_cli_args args = {
        .opts = scrcpy_options_default,
        .help = false,
        .version = false,
    };

    char *argv[] = {"scrcpy", "-v"};

    bool ok = scrcpy_parse_args(&args, 2, argv);
    assert(ok);
    assert(!args.help);
    assert(args.version);
}

static void test_flag_help(void) {
    struct scrcpy_cli_args args = {
        .opts = scrcpy_options_default,
        .help = false,
        .version = false,
    };

    char *argv[] = {"scrcpy", "-v"};

    bool ok = scrcpy_parse_args(&args, 2, argv);
    assert(ok);
    assert(!args.help);
    assert(args.version);
}

static void test_options(void) {
    struct scrcpy_cli_args args = {
        .opts = scrcpy_options_default,
        .help = false,
        .version = false,
    };

    char *argv[] = {
        "scrcpy",
        "--always-on-top",
        "--video-bit-rate", "5M",
        "--crop", "100:200:300:400",
        "--fullscreen",
        "--max-fps", "30",
        "--max-size", "1024",
        // "--no-control" is not compatible with "--turn-screen-off"
        // "--no-playback" is not compatible with "--fulscreen"
        "--port", "1234:1236",
        "--push-target", "/sdcard/Movies",
        "--record", "file",
        "--record-format", "mkv",
        "--serial", "0123456789abcdef",
        "--show-touches",
        "--turn-screen-off",
        "--prefer-text",
        "--window-title", "my device",
        "--window-x", "100",
        "--window-y", "-1",
        "--window-width", "600",
        "--window-height", "0",
        "--window-borderless",
    };

    bool ok = scrcpy_parse_args(&args, ARRAY_LEN(argv), argv);
    assert(ok);

    const struct scrcpy_options *opts = &args.opts;
    assert(opts->always_on_top);
    assert(opts->video_bit_rate == 5000000);
    assert(!strcmp(opts->crop, "100:200:300:400"));
    assert(opts->fullscreen);
    assert(!strcmp(opts->max_fps, "30"));
    assert(opts->max_size == 1024);
    assert(opts->port_range.first == 1234);
    assert(opts->port_range.last == 1236);
    assert(!strcmp(opts->push_target, "/sdcard/Movies"));
    assert(!strcmp(opts->record_filename, "file"));
    assert(opts->record_format == SC_RECORD_FORMAT_MKV);
    assert(!strcmp(opts->serial, "0123456789abcdef"));
    assert(opts->show_touches);
    assert(opts->turn_screen_off);
    assert(opts->key_inject_mode == SC_KEY_INJECT_MODE_TEXT);
    assert(!strcmp(opts->window_title, "my device"));
    assert(opts->window_x == 100);
    assert(opts->window_y == -1);
    assert(opts->window_width == 600);
    assert(opts->window_height == 0);
    assert(opts->window_borderless);
}

static void test_options2(void) {
    struct scrcpy_cli_args args = {
        .opts = scrcpy_options_default,
        .help = false,
        .version = false,
    };

    char *argv[] = {
        "scrcpy",
        "--no-control",
        "--no-playback",
        "--record", "file.mp4", // cannot enable --no-playback without recording
    };

    bool ok = scrcpy_parse_args(&args, ARRAY_LEN(argv), argv);
    assert(ok);

    const struct scrcpy_options *opts = &args.opts;
    assert(!opts->control);
    assert(!opts->video_playback);
    assert(!opts->audio_playback);
    assert(!strcmp(opts->record_filename, "file.mp4"));
    assert(opts->record_format == SC_RECORD_FORMAT_MP4);
}

static void test_max_fps(void) {
    struct scrcpy_cli_args args;
    bool ok;

    char *argv1[] = {"scrcpy", "--max-fps", "30"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv1), argv1);
    assert(ok);
    assert(!strcmp(args.opts.max_fps, "30"));

    char *argv2[] = {"scrcpy", "--max-fps", "30.5"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv2), argv2);
    assert(ok);
    assert(!strcmp(args.opts.max_fps, "30.5"));

    char *argv3[] = {"scrcpy", "--max-fps", "abc"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv3), argv3);
    assert(!ok);

    char *argv4[] = {"scrcpy", "--max-fps", "0"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv4), argv4);
    assert(!ok);

    char *argv5[] = {"scrcpy", "--max-fps", "-1"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv5), argv5);
    assert(!ok);

    char *argv6[] = {"scrcpy", "--max-fps", "30abc"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv6), argv6);
    assert(!ok);

    char *argv7[] = {"scrcpy", "--max-fps", "30."};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv7), argv7);
    assert(!ok);

    char *argv8[] = {"scrcpy", "--max-fps", "30.5.5"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv8), argv8);
    assert(!ok);
}

static void test_angle(void) {
    struct scrcpy_cli_args args;
    bool ok;

    char *argv1[] = {"scrcpy", "--angle", "90"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv1), argv1);
    assert(ok);
    assert(!strcmp(args.opts.angle, "90"));

    char *argv2[] = {"scrcpy", "--angle", "-90"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv2), argv2);
    assert(ok);
    assert(!strcmp(args.opts.angle, "-90"));

    char *argv3[] = {"scrcpy", "--angle", "abc"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv3), argv3);
    assert(!ok);

    char *argv4[] = {"scrcpy", "--angle", "90abc"};
    args.opts = scrcpy_options_default;
    ok = scrcpy_parse_args(&args, ARRAY_LEN(argv4), argv4);
    assert(!ok);
}

static void test_parse_shortcut_mods(void) {
    uint8_t mods;
    bool ok;

    ok = sc_parse_shortcut_mods("lctrl", &mods);
    assert(ok);
    assert(mods == SC_SHORTCUT_MOD_LCTRL);

    ok = sc_parse_shortcut_mods("rctrl,lalt", &mods);
    assert(ok);
    assert(mods == (SC_SHORTCUT_MOD_RCTRL | SC_SHORTCUT_MOD_LALT));

    ok = sc_parse_shortcut_mods("lsuper,rsuper,lctrl", &mods);
    assert(ok);
    assert(mods == (SC_SHORTCUT_MOD_LSUPER
                  | SC_SHORTCUT_MOD_RSUPER
                  | SC_SHORTCUT_MOD_LCTRL));

    ok = sc_parse_shortcut_mods("", &mods);
    assert(!ok);

    ok = sc_parse_shortcut_mods("lctrl+", &mods);
    assert(!ok);

    ok = sc_parse_shortcut_mods("lctrl,", &mods);
    assert(!ok);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_flag_version();
    test_flag_help();
    test_options();
    test_options2();
    test_max_fps();
    test_angle();
    test_parse_shortcut_mods();
    return 0;
}
