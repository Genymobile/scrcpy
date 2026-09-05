#include "common.h"

#include <assert.h>
#include <stdlib.h>
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

static void test_tilde_expansion(void) {
    // A shell does not expand a tilde that follows '=' (e.g. --record=~/x) nor
    // one passed as a literal argument, so path options must resolve a leading
    // "~/" to $HOME themselves.
    setenv("HOME", "/home/tester", 1);

    // "~/..." resolves to "$HOME/..."
    {
        struct scrcpy_cli_args args = {
            .opts = scrcpy_options_default,
            .help = false,
            .version = false,
        };
        char *argv[] = {"scrcpy", "--record", "~/videos/rec.mp4"};
        bool ok = scrcpy_parse_args(&args, ARRAY_LEN(argv), argv);
        assert(ok);
        assert(!strcmp(args.opts.record_filename,
                       "/home/tester/videos/rec.mp4"));
    }

    // A tilde that is not at the start of the path is left untouched.
    {
        struct scrcpy_cli_args args = {
            .opts = scrcpy_options_default,
            .help = false,
            .version = false,
        };
        char *argv[] = {"scrcpy", "--record", "/tmp/a~b.mp4"};
        bool ok = scrcpy_parse_args(&args, ARRAY_LEN(argv), argv);
        assert(ok);
        assert(!strcmp(args.opts.record_filename, "/tmp/a~b.mp4"));
    }
}

static void test_daemon_accepts_vpx(void) {
    const char *codecs[] = {"vp8", "vp9"};
    const enum sc_codec expected[] = {SC_CODEC_VP8, SC_CODEC_VP9};
    for (size_t i = 0; i < ARRAY_LEN(codecs); ++i) {
        struct scrcpy_cli_args args = {
            .opts = scrcpy_options_default,
            .help = false,
            .version = false,
        };
        char *argv[] = {
            "scrcpy",
            "--daemon-port", "27183",
            "--no-window",
            "--video-codec", (char *) codecs[i],
        };
        bool ok = scrcpy_parse_args(&args, ARRAY_LEN(argv), argv);
        assert(ok);
        assert(args.opts.video_codec == expected[i]);
        assert(args.opts.video);
        assert(!args.opts.audio);
    }
}

static void test_mirror_resolves_input_defaults(void) {
    struct scrcpy_cli_args args = {
        .opts = scrcpy_options_default,
        .help = false,
        .version = false,
    };
    char *argv[] = {"scrcpy", "--client-port", "27183"};

    bool ok = scrcpy_parse_args(&args, ARRAY_LEN(argv), argv);
    assert(ok);
    assert(args.opts.mirror);
    assert(args.opts.keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_SDK);
    assert(args.opts.mouse_input_mode == SC_MOUSE_INPUT_MODE_SDK);
    // The daemon hello owns the flex-display capability, so mirror resolves
    // AUTO only after connecting.
    assert(args.opts.render_fit == SC_RENDER_FIT_AUTO);
    assert(args.opts.mouse_bindings.pri.right_click == SC_MOUSE_BINDING_BACK);
    assert(args.opts.mouse_bindings.sec.right_click == SC_MOUSE_BINDING_CLICK);
}

static void test_mirror_rejects_gamepad(void) {
    struct scrcpy_cli_args args = {
        .opts = scrcpy_options_default,
        .help = false,
        .version = false,
    };
    char *argv[] = {
        "scrcpy", "--client-port", "27183", "--gamepad=uhid",
    };

    bool ok = scrcpy_parse_args(&args, ARRAY_LEN(argv), argv);
    assert(!ok);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_flag_version();
    test_flag_help();
    test_options();
    test_options2();
    test_parse_shortcut_mods();
    test_tilde_expansion();
    test_daemon_accepts_vpx();
    test_mirror_resolves_input_defaults();
    test_mirror_rejects_gamepad();
    return 0;
}
