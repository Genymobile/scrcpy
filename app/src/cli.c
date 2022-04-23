#include "cli.h"

#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "options.h"
#include "util/log.h"
#include "util/net.h"
#include "util/str.h"
#include "util/strbuf.h"
#include "util/term.h"

#define STR_IMPL_(x) #x
#define STR(x) STR_IMPL_(x)

#define OPT_RENDER_EXPIRED_FRAMES  1000
#define OPT_WINDOW_TITLE           1001
#define OPT_PUSH_TARGET            1002
#define OPT_ALWAYS_ON_TOP          1003
#define OPT_CROP                   1004
#define OPT_RECORD_FORMAT          1005
#define OPT_PREFER_TEXT            1006
#define OPT_WINDOW_X               1007
#define OPT_WINDOW_Y               1008
#define OPT_WINDOW_WIDTH           1009
#define OPT_WINDOW_HEIGHT          1010
#define OPT_WINDOW_BORDERLESS      1011
#define OPT_MAX_FPS                1012
#define OPT_LOCK_VIDEO_ORIENTATION 1013
#define OPT_DISPLAY_ID             1014
#define OPT_ROTATION               1015
#define OPT_RENDER_DRIVER          1016
#define OPT_NO_MIPMAPS             1017
#define OPT_CODEC_OPTIONS          1018
#define OPT_FORCE_ADB_FORWARD      1019
#define OPT_DISABLE_SCREENSAVER    1020
#define OPT_SHORTCUT_MOD           1021
#define OPT_NO_KEY_REPEAT          1022
#define OPT_FORWARD_ALL_CLICKS     1023
#define OPT_LEGACY_PASTE           1024
#define OPT_ENCODER_NAME           1025
#define OPT_POWER_OFF_ON_CLOSE     1026
#define OPT_V4L2_SINK              1027
#define OPT_DISPLAY_BUFFER         1028
#define OPT_V4L2_BUFFER            1029
#define OPT_TUNNEL_HOST            1030
#define OPT_TUNNEL_PORT            1031
#define OPT_NO_CLIPBOARD_AUTOSYNC  1032
#define OPT_TCPIP                  1033
#define OPT_RAW_KEY_EVENTS         1034
#define OPT_NO_DOWNSIZE_ON_ERROR   1035
#define OPT_OTG                    1036
#define OPT_NO_CLEANUP             1037
#define OPT_PRINT_FPS              1038
#define OPT_NO_POWER_ON            1039

struct sc_option {
    char shortopt;
    int longopt_id; // either shortopt or longopt_id is non-zero
    const char *longopt;
    // no argument:       argdesc == NULL && !optional_arg
    // optional argument: argdesc != NULL && optional_arg
    // required argument: argdesc != NULL && !optional_arg
    const char *argdesc;
    bool optional_arg;
    const char *text; // if NULL, the option does not appear in the help
};

#define MAX_EQUIVALENT_SHORTCUTS 3
struct sc_shortcut {
    const char *shortcuts[MAX_EQUIVALENT_SHORTCUTS + 1];
    const char *text;
};

struct sc_envvar {
    const char *name;
    const char *text;
};

struct sc_exit_status {
    unsigned value;
    const char *text;
};

struct sc_getopt_adapter {
    char *optstring;
    struct option *longopts;
};

static const struct sc_option options[] = {
    {
        .longopt_id = OPT_ALWAYS_ON_TOP,
        .longopt = "always-on-top",
        .text = "Make scrcpy window always on top (above other windows).",
    },
    {
        .shortopt = 'b',
        .longopt = "bit-rate",
        .argdesc = "value",
        .text = "Encode the video at the gitven bit-rate, expressed in bits/s. "
                "Unit suffixes are supported: 'K' (x1000) and 'M' (x1000000).\n"
                "Default is " STR(DEFAULT_BIT_RATE) ".",
    },
    {
        .longopt_id = OPT_CODEC_OPTIONS,
        .longopt = "codec-options",
        .argdesc = "key[:type]=value[,...]",
        .text = "Set a list of comma-separated key:type=value options for the "
                "device encoder.\n"
                "The possible values for 'type' are 'int' (default), 'long', "
                "'float' and 'string'.\n"
                "The list of possible codec options is available in the "
                "Android documentation: "
                "<https://d.android.com/reference/android/media/MediaFormat>",
    },
    {
        .longopt_id = OPT_CROP,
        .longopt = "crop",
        .argdesc = "width:height:x:y",
        .text = "Crop the device screen on the server.\n"
                "The values are expressed in the device natural orientation "
                "(typically, portrait for a phone, landscape for a tablet). "
                "Any --max-size value is computed on the cropped size.",
    },
    {
        .shortopt = 'd',
        .longopt = "select-usb",
        .text = "Use USB device (if there is exactly one, like adb -d).\n"
                "Also see -e (--select-tcpip).",
    },
    {
        .longopt_id = OPT_DISABLE_SCREENSAVER,
        .longopt = "disable-screensaver",
        .text = "Disable screensaver while scrcpy is running.",
    },
    {
        .longopt_id = OPT_DISPLAY_ID,
        .longopt = "display",
        .argdesc = "id",
        .text = "Specify the display id to mirror.\n"
                "The list of possible display ids can be listed by:\n"
                "    adb shell dumpsys display\n"
                "(search \"mDisplayId=\" in the output)\n"
                "Default is 0.",
    },
    {
        .longopt_id = OPT_DISPLAY_BUFFER,
        .longopt = "display-buffer",
        .argdesc = "ms",
        .text = "Add a buffering delay (in milliseconds) before displaying. "
                "This increases latency to compensate for jitter.\n"
                "Default is 0 (no buffering).",
    },
    {
        .shortopt = 'e',
        .longopt = "select-tcpip",
        .text = "Use TCP/IP device (if there is exactly one, like adb -e).\n"
                "Also see -d (--select-usb).",
    },
    {
        .longopt_id = OPT_ENCODER_NAME,
        .longopt = "encoder",
        .argdesc = "name",
        .text = "Use a specific MediaCodec encoder (must be a H.264 encoder).",
    },
    {
        .longopt_id = OPT_FORCE_ADB_FORWARD,
        .longopt = "force-adb-forward",
        .text = "Do not attempt to use \"adb reverse\" to connect to the "
                "device.",
    },
    {
        .longopt_id = OPT_FORWARD_ALL_CLICKS,
        .longopt = "forward-all-clicks",
        .text = "By default, right-click triggers BACK (or POWER on) and "
                "middle-click triggers HOME. This option disables these "
                "shortcuts and forwards the clicks to the device instead.",
    },
    {
        .shortopt = 'f',
        .longopt = "fullscreen",
        .text = "Start in fullscreen.",
    },
    {
        .shortopt = 'K',
        .longopt = "hid-keyboard",
        .text = "Simulate a physical keyboard by using HID over AOAv2.\n"
                "It provides a better experience for IME users, and allows to "
                "generate non-ASCII characters, contrary to the default "
                "injection method.\n"
                "It may only work over USB.\n"
                "The keyboard layout must be configured (once and for all) on "
                "the device, via Settings -> System -> Languages and input -> "
                "Physical keyboard. This settings page can be started "
                "directly: `adb shell am start -a "
                "android.settings.HARD_KEYBOARD_SETTINGS`.\n"
                "However, the option is only available when the HID keyboard "
                "is enabled (or a physical keyboard is connected).\n"
                "Also see --hid-mouse.",
    },
    {
        .shortopt = 'h',
        .longopt = "help",
        .text = "Print this help.",
    },
    {
        .longopt_id = OPT_LEGACY_PASTE,
        .longopt = "legacy-paste",
        .text = "Inject computer clipboard text as a sequence of key events "
                "on Ctrl+v (like MOD+Shift+v).\n"
                "This is a workaround for some devices not behaving as "
                "expected when setting the device clipboard programmatically.",
    },
    {
        .longopt_id = OPT_LOCK_VIDEO_ORIENTATION,
        .longopt = "lock-video-orientation",
        .argdesc = "value",
        .optional_arg = true,
        .text = "Lock video orientation to value.\n"
                "Possible values are \"unlocked\", \"initial\" (locked to the "
                "initial orientation), 0, 1, 2 and 3. Natural device "
                "orientation is 0, and each increment adds a 90 degrees "
                "rotation counterclockwise.\n"
                "Default is \"unlocked\".\n"
                "Passing the option without argument is equivalent to passing "
                "\"initial\".",
    },
    {
        .longopt_id = OPT_MAX_FPS,
        .longopt = "max-fps",
        .argdesc = "value",
        .text = "Limit the frame rate of screen capture (officially supported "
                "since Android 10, but may work on earlier versions).",
    },
    {
        .shortopt = 'M',
        .longopt = "hid-mouse",
        .text = "Simulate a physical mouse by using HID over AOAv2.\n"
                "In this mode, the computer mouse is captured to control the "
                "device directly (relative mouse mode).\n"
                "LAlt, LSuper or RSuper toggle the capture mode, to give "
                "control of the mouse back to the computer.\n"
                "It may only work over USB.\n"
                "Also see --hid-keyboard.",
    },
    {
        .shortopt = 'm',
        .longopt = "max-size",
        .argdesc = "value",
        .text = "Limit both the width and height of the video to value. The "
                "other dimension is computed so that the device aspect-ratio "
                "is preserved.\n"
                "Default is 0 (unlimited).",
    },
    {
        .longopt_id = OPT_NO_CLEANUP,
        .longopt = "no-cleanup",
        .text = "By default, scrcpy removes the server binary from the device "
                "and restores the device state (show touches, stay awake and "
                "power mode) on exit.\n"
                "This option disables this cleanup."
    },
    {
        .longopt_id = OPT_NO_CLIPBOARD_AUTOSYNC,
        .longopt = "no-clipboard-autosync",
        .text = "By default, scrcpy automatically synchronizes the computer "
                "clipboard to the device clipboard before injecting Ctrl+v, "
                "and the device clipboard to the computer clipboard whenever "
                "it changes.\n"
                "This option disables this automatic synchronization."
    },
    {
        .longopt_id = OPT_NO_DOWNSIZE_ON_ERROR,
        .longopt = "no-downsize-on-error",
        .text = "By default, on MediaCodec error, scrcpy automatically tries "
                "again with a lower definition.\n"
                "This option disables this behavior.",
    },
    {
        .shortopt = 'n',
        .longopt = "no-control",
        .text = "Disable device control (mirror the device in read-only).",
    },
    {
        .shortopt = 'N',
        .longopt = "no-display",
        .text = "Do not display device (only when screen recording or V4L2 "
                "sink is enabled).",
    },
    {
        .longopt_id = OPT_NO_KEY_REPEAT,
        .longopt = "no-key-repeat",
        .text = "Do not forward repeated key events when a key is held down.",
    },
    {
        .longopt_id = OPT_NO_MIPMAPS,
        .longopt = "no-mipmaps",
        .text = "If the renderer is OpenGL 3.0+ or OpenGL ES 2.0+, then "
                "mipmaps are automatically generated to improve downscaling "
                "quality. This option disables the generation of mipmaps.",
    },
    {
        .longopt_id = OPT_NO_POWER_ON,
        .longopt = "no-power-on",
        .text = "Do not power on the device on start.",
    },
    {
        .longopt_id = OPT_OTG,
        .longopt = "otg",
        .text = "Run in OTG mode: simulate physical keyboard and mouse, "
                "as if the computer keyboard and mouse were plugged directly "
                "to the device via an OTG cable.\n"
                "In this mode, adb (USB debugging) is not necessary, and "
                "mirroring is disabled.\n"
                "LAlt, LSuper or RSuper toggle the mouse capture mode, to give "
                "control of the mouse back to the computer.\n"
                "If any of --hid-keyboard or --hid-mouse is set, only enable "
                "keyboard or mouse respectively, otherwise enable both.\n"
                "It may only work over USB.\n"
                "See --hid-keyboard and --hid-mouse.",
    },
    {
        .shortopt = 'p',
        .longopt = "port",
        .argdesc = "port[:port]",
        .text = "Set the TCP port (range) used by the client to listen.\n"
                "Default is " STR(DEFAULT_LOCAL_PORT_RANGE_FIRST) ":"
                              STR(DEFAULT_LOCAL_PORT_RANGE_LAST) ".",
    },
    {
        .longopt_id = OPT_POWER_OFF_ON_CLOSE,
        .longopt = "power-off-on-close",
        .text = "Turn the device screen off when closing scrcpy.",
    },
    {
        .longopt_id = OPT_PREFER_TEXT,
        .longopt = "prefer-text",
        .text = "Inject alpha characters and space as text events instead of "
                "key events.\n"
                "This avoids issues when combining multiple keys to enter a "
                "special character, but breaks the expected behavior of alpha "
                "keys in games (typically WASD).",
    },
    {
        .longopt_id = OPT_PRINT_FPS,
        .longopt = "print-fps",
        .text = "Start FPS counter, to print framerate logs to the console. "
                "It can be started or stopped at any time with MOD+i.",
    },
    {
        .longopt_id = OPT_PUSH_TARGET,
        .longopt = "push-target",
        .argdesc = "path",
        .text = "Set the target directory for pushing files to the device by "
                "drag & drop. It is passed as is to \"adb push\".\n"
                "Default is \"/sdcard/Download/\".",
    },
    {
        .longopt_id = OPT_RAW_KEY_EVENTS,
        .longopt = "raw-key-events",
        .text = "Inject key events for all input keys, and ignore text events."
    },
    {
        .shortopt = 'r',
        .longopt = "record",
        .argdesc = "file.mp4",
        .text = "Record screen to file.\n"
                "The format is determined by the --record-format option if "
                "set, or by the file extension (.mp4 or .mkv).",
    },
    {
        .longopt_id = OPT_RECORD_FORMAT,
        .longopt = "record-format",
        .argdesc = "format",
        .text = "Force recording format (either mp4 or mkv).",
    },
    {
        .longopt_id = OPT_RENDER_DRIVER,
        .longopt = "render-driver",
        .argdesc = "name",
        .text = "Request SDL to use the given render driver (this is just a "
                "hint).\n"
                "Supported names are currently \"direct3d\", \"opengl\", "
                "\"opengles2\", \"opengles\", \"metal\" and \"software\".\n"
                "<https://wiki.libsdl.org/SDL_HINT_RENDER_DRIVER>",
    },
    {
        // deprecated
        .longopt_id = OPT_RENDER_EXPIRED_FRAMES,
        .longopt = "render-expired-frames",
    },
    {
        .longopt_id = OPT_ROTATION,
        .longopt = "rotation",
        .argdesc = "value",
        .text = "Set the initial display rotation.\n"
                "Possible values are 0, 1, 2 and 3. Each increment adds a 90 "
                "degrees rotation counterclockwise.",
    },
    {
        .shortopt = 's',
        .longopt = "serial",
        .argdesc = "serial",
        .text = "The device serial number. Mandatory only if several devices "
                "are connected to adb.",
    },
    {
        .longopt_id = OPT_SHORTCUT_MOD,
        .longopt = "shortcut-mod",
        .argdesc = "key[+...][,...]",
        .text = "Specify the modifiers to use for scrcpy shortcuts.\n"
                "Possible keys are \"lctrl\", \"rctrl\", \"lalt\", \"ralt\", "
                "\"lsuper\" and \"rsuper\".\n"
                "A shortcut can consist in several keys, separated by '+'. "
                "Several shortcuts can be specified, separated by ','.\n"
                "For example, to use either LCtrl+LAlt or LSuper for scrcpy "
                "shortcuts, pass \"lctrl+lalt,lsuper\".\n"
                "Default is \"lalt,lsuper\" (left-Alt or left-Super).",
    },
    {
        .shortopt = 'S',
        .longopt = "turn-screen-off",
        .text = "Turn the device screen off immediately.",
    },
    {
        .shortopt = 't',
        .longopt = "show-touches",
        .text = "Enable \"show touches\" on start, restore the initial value "
                "on exit.\n"
                "It only shows physical touches (not clicks from scrcpy).",
    },
    {
        .longopt_id = OPT_TCPIP,
        .longopt = "tcpip",
        .argdesc = "ip[:port]",
        .optional_arg = true,
        .text = "Configure and reconnect the device over TCP/IP.\n"
                "If a destination address is provided, then scrcpy connects to "
                "this address before starting. The device must listen on the "
                "given TCP port (default is 5555).\n"
                "If no destination address is provided, then scrcpy attempts "
                "to find the IP address of the current device (typically "
                "connected over USB), enables TCP/IP mode, then connects to "
                "this address before starting.",
    },
    {
        .longopt_id = OPT_TUNNEL_HOST,
        .longopt = "tunnel-host",
        .argdesc = "ip",
        .text = "Set the IP address of the adb tunnel to reach the scrcpy "
                "server. This option automatically enables "
                "--force-adb-forward.\n"
                "Default is localhost.",
    },
    {
        .longopt_id = OPT_TUNNEL_PORT,
        .longopt = "tunnel-port",
        .argdesc = "port",
        .text = "Set the TCP port of the adb tunnel to reach the scrcpy "
                "server. This option automatically enables "
                "--force-adb-forward.\n"
                "Default is 0 (not forced): the local port used for "
                "establishing the tunnel will be used.",
    },
    {
        .longopt_id = OPT_V4L2_SINK,
        .longopt = "v4l2-sink",
        .argdesc = "/dev/videoN",
        .text = "Output to v4l2loopback device.\n"
                "It requires to lock the video orientation (see "
                "--lock-video-orientation).\n"
                "This feature is only available on Linux.",
    },
    {
        .longopt_id = OPT_V4L2_BUFFER,
        .longopt = "v4l2-buffer",
        .argdesc = "ms",
        .text = "Add a buffering delay (in milliseconds) before pushing "
                "frames. This increases latency to compensate for jitter.\n"
                "This option is similar to --display-buffer, but specific to "
                "V4L2 sink.\n"
                "Default is 0 (no buffering).\n"
                "This option is only available on Linux.",
    },
    {
        .shortopt = 'V',
        .longopt = "verbosity",
        .argdesc = "value",
        .text = "Set the log level (verbose, debug, info, warn or error).\n"
#ifndef NDEBUG
                "Default is debug.",
#else
                "Default is info.",
#endif
    },
    {
        .shortopt = 'v',
        .longopt = "version",
        .text = "Print the version of scrcpy.",
    },
    {
        .shortopt = 'w',
        .longopt = "stay-awake",
        .text = "Keep the device on while scrcpy is running, when the device "
                "is plugged in.",
    },
    {
        .longopt_id = OPT_WINDOW_BORDERLESS,
        .longopt = "window-borderless",
        .text = "Disable window decorations (display borderless window)."
    },
    {
        .longopt_id = OPT_WINDOW_TITLE,
        .longopt = "window-title",
        .argdesc = "text",
        .text = "Set a custom window title.",
    },
    {
        .longopt_id = OPT_WINDOW_X,
        .longopt = "window-x",
        .argdesc = "value",
        .text = "Set the initial window horizontal position.\n"
                "Default is \"auto\".",
    },
    {
        .longopt_id = OPT_WINDOW_Y,
        .longopt = "window-y",
        .argdesc = "value",
        .text = "Set the initial window vertical position.\n"
                "Default is \"auto\".",
    },
    {
        .longopt_id = OPT_WINDOW_WIDTH,
        .longopt = "window-width",
        .argdesc = "value",
        .text = "Set the initial window width.\n"
                "Default is 0 (automatic).",
    },
    {
        .longopt_id = OPT_WINDOW_HEIGHT,
        .longopt = "window-height",
        .argdesc = "value",
        .text = "Set the initial window height.\n"
                "Default is 0 (automatic).",
    },
};

static const struct sc_shortcut shortcuts[] = {
    {
        .shortcuts = { "MOD+f" },
        .text = "Switch fullscreen mode",
    },
    {
        .shortcuts = { "MOD+Left" },
        .text = "Rotate display left",
    },
    {
        .shortcuts = { "MOD+Right" },
        .text = "Rotate display right",
    },
    {
        .shortcuts = { "MOD+g" },
        .text = "Resize window to 1:1 (pixel-perfect)",
    },
    {
        .shortcuts = { "MOD+w", "Double-click on black borders" },
        .text = "Resize window to remove black borders",
    },
    {
        .shortcuts = { "MOD+h", "Middle-click" },
        .text = "Click on HOME",
    },
    {
        .shortcuts = {
            "MOD+b",
            "MOD+Backspace",
            "Right-click (when screen is on)",
        },
        .text = "Click on BACK",
    },
    {
        .shortcuts = { "MOD+s", "4th-click" },
        .text = "Click on APP_SWITCH",
    },
    {
        .shortcuts = { "MOD+m" },
        .text = "Click on MENU",
    },
    {
        .shortcuts = { "MOD+Up" },
        .text = "Click on VOLUME_UP",
    },
    {
        .shortcuts = { "MOD+Down" },
        .text = "Click on VOLUME_DOWN",
    },
    {
        .shortcuts = { "MOD+p" },
        .text = "Click on POWER (turn screen on/off)",
    },
    {
        .shortcuts = { "Right-click (when screen is off)" },
        .text = "Power on",
    },
    {
        .shortcuts = { "MOD+o" },
        .text = "Turn device screen off (keep mirroring)",
    },
    {
        .shortcuts = { "MOD+Shift+o" },
        .text = "Turn device screen on",
    },
    {
        .shortcuts = { "MOD+r" },
        .text = "Rotate device screen",
    },
    {
        .shortcuts = { "MOD+n", "5th-click" },
        .text = "Expand notification panel",
    },
    {
        .shortcuts = { "MOD+Shift+n" },
        .text = "Collapse notification panel",
    },
    {
        .shortcuts = { "MOD+c" },
        .text = "Copy to clipboard (inject COPY keycode, Android >= 7 only)",
    },
    {
        .shortcuts = { "MOD+x" },
        .text = "Cut to clipboard (inject CUT keycode, Android >= 7 only)",
    },
    {
        .shortcuts = { "MOD+v" },
        .text = "Copy computer clipboard to device, then paste (inject PASTE "
                "keycode, Android >= 7 only)",
    },
    {
        .shortcuts = { "MOD+Shift+v" },
        .text = "Inject computer clipboard text as a sequence of key events",
    },
    {
        .shortcuts = { "MOD+i" },
        .text = "Enable/disable FPS counter (print frames/second in logs)",
    },
    {
        .shortcuts = { "Ctrl+click-and-move" },
        .text = "Pinch-to-zoom from the center of the screen",
    },
    {
        .shortcuts = { "Drag & drop APK file" },
        .text = "Install APK from computer",
    },
    {
        .shortcuts = { "Drag & drop non-APK file" },
        .text = "Push file to device (see --push-target)",
    },
};

static const struct sc_envvar envvars[] = {
    {
        .name = "ADB",
        .text = "Path to adb executable",
    },
    {
        .name = "ANDROID_SERIAL",
        .text = "Device serial to use if no selector (-s, -d, -e or "
                "--tcpip=<addr>) is specified",
    },
    {
        .name = "SCRCPY_ICON_PATH",
        .text = "Path to the program icon",
    },
    {
        .name = "SCRCPY_SERVER_PATH",
        .text = "Path to the server binary",
    },
};

static const struct sc_exit_status exit_statuses[] = {
    {
        .value = 0,
        .text = "Normal program termination",
    },
    {
        .value = 1,
        .text = "Start failure",
    },
    {
        .value = 2,
        .text = "Device disconnected while running",
    },
};

static char *
sc_getopt_adapter_create_optstring(void) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 64)) {
        return false;
    }

    for (size_t i = 0; i < ARRAY_LEN(options); ++i) {
        const struct sc_option *opt = &options[i];
        if (opt->shortopt) {
            if (!sc_strbuf_append_char(&buf, opt->shortopt)) {
                goto error;
            }
            // If there is an argument, add ':'
            if (opt->argdesc) {
                if (!sc_strbuf_append_char(&buf, ':')) {
                    goto error;
                }
                // If the argument is optional, add another ':'
                if (opt->optional_arg && !sc_strbuf_append_char(&buf, ':')) {
                    goto error;
                }
            }
        }
    }

    return buf.s;

error:
    free(buf.s);
    return NULL;
}

static struct option *
sc_getopt_adapter_create_longopts(void) {
    struct option *longopts =
        malloc((ARRAY_LEN(options) + 1) * sizeof(*longopts));
    if (!longopts) {
        LOG_OOM();
        return NULL;
    }

    size_t out_idx = 0;
    for (size_t i = 0; i < ARRAY_LEN(options); ++i) {
        const struct sc_option *in = &options[i];

        // If longopt_id is set, then longopt must be set
        assert(!in->longopt_id || in->longopt);

        if (!in->longopt) {
            // The longopts array must only contain long options
            continue;
        }
        struct option *out = &longopts[out_idx++];

        out->name = in->longopt;

        if (!in->argdesc) {
            assert(!in->optional_arg);
            out->has_arg = no_argument;
        } else if (in->optional_arg) {
            out->has_arg = optional_argument;
        } else {
            out->has_arg = required_argument;
        }

        out->flag = NULL;

        // Either shortopt or longopt_id is set, but not both
        assert(!!in->shortopt ^ !!in->longopt_id);
        out->val = in->shortopt ? in->shortopt : in->longopt_id;
    }

    // The array must be terminated by a NULL item
    longopts[out_idx] = (struct option) {0};

    return longopts;
}

static bool
sc_getopt_adapter_init(struct sc_getopt_adapter *adapter) {
    adapter->optstring = sc_getopt_adapter_create_optstring();
    if (!adapter->optstring) {
        return false;
    }

    adapter->longopts = sc_getopt_adapter_create_longopts();
    if (!adapter->longopts) {
        free(adapter->optstring);
        return false;
    }

    return true;
}

static void
sc_getopt_adapter_destroy(struct sc_getopt_adapter *adapter) {
    free(adapter->optstring);
    free(adapter->longopts);
}

static void
print_option_usage_header(const struct sc_option *opt) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 64)) {
        goto error;
    }

    bool ok = true;
    (void) ok; // only used for assertions

    if (opt->shortopt) {
        ok = sc_strbuf_append_char(&buf, '-');
        assert(ok);

        ok = sc_strbuf_append_char(&buf, opt->shortopt);
        assert(ok);

        if (opt->longopt) {
            ok = sc_strbuf_append_staticstr(&buf, ", ");
            assert(ok);
        }
    }

    if (opt->longopt) {
        ok = sc_strbuf_append_staticstr(&buf, "--");
        assert(ok);

        if (!sc_strbuf_append_str(&buf, opt->longopt)) {
            goto error;
        }
    }

    if (opt->argdesc) {
        if (opt->optional_arg && !sc_strbuf_append_char(&buf, '[')) {
            goto error;
        }

        if (!sc_strbuf_append_char(&buf, '=')) {
            goto error;
        }

        if (!sc_strbuf_append_str(&buf, opt->argdesc)) {
            goto error;
        }

        if (opt->optional_arg && !sc_strbuf_append_char(&buf, ']')) {
            goto error;
        }
    }

    printf("\n    %s\n", buf.s);
    free(buf.s);
    return;

error:
    printf("<ERROR>\n");
}

static void
print_option_usage(const struct sc_option *opt, unsigned cols) {
    assert(cols > 8); // sc_str_wrap_lines() requires indent < columns

    if (!opt->text) {
        // Option not documented in help (for example because it is deprecated)
        return;
    }

    print_option_usage_header(opt);

    char *text = sc_str_wrap_lines(opt->text, cols, 8);
    if (!text) {
        printf("<ERROR>\n");
        return;
    }

    printf("%s\n", text);
    free(text);
}

static void
print_shortcuts_intro(unsigned cols) {
    char *intro = sc_str_wrap_lines(
        "In the following list, MOD is the shortcut modifier. By default, it's "
        "(left) Alt or (left) Super, but it can be configured by "
        "--shortcut-mod (see above).", cols, 4);
    if (!intro) {
        printf("<ERROR>\n");
        return;
    }

    printf("\n%s\n", intro);
    free(intro);
}

static void
print_shortcut(const struct sc_shortcut *shortcut, unsigned cols) {
    assert(cols > 8); // sc_str_wrap_lines() requires indent < columns
    assert(shortcut->shortcuts[0]); // At least one shortcut
    assert(shortcut->text);

    printf("\n");

    unsigned i = 0;
    while (shortcut->shortcuts[i]) {
        printf("    %s\n", shortcut->shortcuts[i]);
        ++i;
    };

    char *text = sc_str_wrap_lines(shortcut->text, cols, 8);
    if (!text) {
        printf("<ERROR>\n");
        return;
    }

    printf("%s\n", text);
    free(text);
}

static void
print_envvar(const struct sc_envvar *envvar, unsigned cols) {
    assert(cols > 8); // sc_str_wrap_lines() requires indent < columns
    assert(envvar->name);
    assert(envvar->text);

    printf("\n    %s\n", envvar->name);
    char *text = sc_str_wrap_lines(envvar->text, cols, 8);
    if (!text) {
        printf("<ERROR>\n");
        return;
    }

    printf("%s\n", text);
    free(text);
}

static void
print_exit_status(const struct sc_exit_status *status, unsigned cols) {
    assert(cols > 8); // sc_str_wrap_lines() requires indent < columns
    assert(status->text);

    // The text starts at 9: 4 ident spaces, 3 chars for numeric value, 2 spaces
    char *text = sc_str_wrap_lines(status->text, cols, 9);
    if (!text) {
        printf("<ERROR>\n");
        return;
    }

    assert(strlen(text) >= 9); // Contains at least the initial identation

    // text + 9 to remove the initial indentation
    printf("    %3d  %s\n", status->value, text + 9);
    free(text);
}

void
scrcpy_print_usage(const char *arg0) {
#define SC_TERM_COLS_DEFAULT 80
    unsigned cols;

    if (!isatty(STDERR_FILENO)) {
        // Not a tty
        cols = SC_TERM_COLS_DEFAULT;
    } else {
        bool ok = sc_term_get_size(NULL, &cols);
        if (!ok) {
            // Could not get the terminal size
            cols = SC_TERM_COLS_DEFAULT;
        }
        if (cols < 20) {
            // Do not accept a too small value
            cols = 20;
        }
    }

    printf("Usage: %s [options]\n\n"
            "Options:\n", arg0);
    for (size_t i = 0; i < ARRAY_LEN(options); ++i) {
        print_option_usage(&options[i], cols);
    }

    // Print shortcuts section
    printf("\nShortcuts:\n");
    print_shortcuts_intro(cols);
    for (size_t i = 0; i < ARRAY_LEN(shortcuts); ++i) {
        print_shortcut(&shortcuts[i], cols);
    }

    // Print environment variables section
    printf("\nEnvironment variables:\n");
    for (size_t i = 0; i < ARRAY_LEN(envvars); ++i) {
        print_envvar(&envvars[i], cols);
    }

    printf("\nExit status:\n\n");
    for (size_t i = 0; i < ARRAY_LEN(exit_statuses); ++i) {
        print_exit_status(&exit_statuses[i], cols);
    }
}

static bool
parse_integer_arg(const char *s, long *out, bool accept_suffix, long min,
                  long max, const char *name) {
    long value;
    bool ok;
    if (accept_suffix) {
        ok = sc_str_parse_integer_with_suffix(s, &value);
    } else {
        ok = sc_str_parse_integer(s, &value);
    }
    if (!ok) {
        LOGE("Could not parse %s: %s", name, s);
        return false;
    }

    if (value < min || value > max) {
        LOGE("Could not parse %s: value (%ld) out-of-range (%ld; %ld)",
             name, value, min, max);
        return false;
    }

    *out = value;
    return true;
}

static size_t
parse_integers_arg(const char *s, size_t max_items, long *out, long min,
                   long max, const char *name) {
    size_t count = sc_str_parse_integers(s, ':', max_items, out);
    if (!count) {
        LOGE("Could not parse %s: %s", name, s);
        return 0;
    }

    for (size_t i = 0; i < count; ++i) {
        long value = out[i];
        if (value < min || value > max) {
            LOGE("Could not parse %s: value (%ld) out-of-range (%ld; %ld)",
                 name, value, min, max);
            return 0;
        }
    }

    return count;
}

static bool
parse_bit_rate(const char *s, uint32_t *bit_rate) {
    long value;
    // long may be 32 bits (it is the case on mingw), so do not use more than
    // 31 bits (long is signed)
    bool ok = parse_integer_arg(s, &value, true, 0, 0x7FFFFFFF, "bit-rate");
    if (!ok) {
        return false;
    }

    *bit_rate = (uint32_t) value;
    return true;
}

static bool
parse_max_size(const char *s, uint16_t *max_size) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 0xFFFF, "max size");
    if (!ok) {
        return false;
    }

    *max_size = (uint16_t) value;
    return true;
}

static bool
parse_max_fps(const char *s, uint16_t *max_fps) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 1000, "max fps");
    if (!ok) {
        return false;
    }

    *max_fps = (uint16_t) value;
    return true;
}

static bool
parse_buffering_time(const char *s, sc_tick *tick) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 0x7FFFFFFF,
                                "buffering time");
    if (!ok) {
        return false;
    }

    *tick = SC_TICK_FROM_MS(value);
    return true;
}

static bool
parse_lock_video_orientation(const char *s,
                             enum sc_lock_video_orientation *lock_mode) {
    if (!s || !strcmp(s, "initial")) {
        // Without argument, lock the initial orientation
        *lock_mode = SC_LOCK_VIDEO_ORIENTATION_INITIAL;
        return true;
    }

    if (!strcmp(s, "unlocked")) {
        *lock_mode = SC_LOCK_VIDEO_ORIENTATION_UNLOCKED;
        return true;
    }

    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 3,
                                "lock video orientation");
    if (!ok) {
        return false;
    }

    *lock_mode = (enum sc_lock_video_orientation) value;
    return true;
}

static bool
parse_rotation(const char *s, uint8_t *rotation) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 3, "rotation");
    if (!ok) {
        return false;
    }

    *rotation = (uint8_t) value;
    return true;
}

static bool
parse_window_position(const char *s, int16_t *position) {
    // special value for "auto"
    static_assert(SC_WINDOW_POSITION_UNDEFINED == -0x8000, "unexpected value");

    if (!strcmp(s, "auto")) {
        *position = SC_WINDOW_POSITION_UNDEFINED;
        return true;
    }

    long value;
    bool ok = parse_integer_arg(s, &value, false, -0x7FFF, 0x7FFF,
                                "window position");
    if (!ok) {
        return false;
    }

    *position = (int16_t) value;
    return true;
}

static bool
parse_window_dimension(const char *s, uint16_t *dimension) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 0xFFFF,
                                "window dimension");
    if (!ok) {
        return false;
    }

    *dimension = (uint16_t) value;
    return true;
}

static bool
parse_port_range(const char *s, struct sc_port_range *port_range) {
    long values[2];
    size_t count = parse_integers_arg(s, 2, values, 0, 0xFFFF, "port");
    if (!count) {
        return false;
    }

    uint16_t v0 = (uint16_t) values[0];
    if (count == 1) {
        port_range->first = v0;
        port_range->last = v0;
        return true;
    }

    assert(count == 2);
    uint16_t v1 = (uint16_t) values[1];
    if (v0 < v1) {
        port_range->first = v0;
        port_range->last = v1;
    } else {
        port_range->first = v1;
        port_range->last = v0;
    }

    return true;
}

static bool
parse_display_id(const char *s, uint32_t *display_id) {
    long value;
    bool ok = parse_integer_arg(s, &value, false, 0, 0x7FFFFFFF, "display id");
    if (!ok) {
        return false;
    }

    *display_id = (uint32_t) value;
    return true;
}

static bool
parse_log_level(const char *s, enum sc_log_level *log_level) {
    if (!strcmp(s, "verbose")) {
        *log_level = SC_LOG_LEVEL_VERBOSE;
        return true;
    }

    if (!strcmp(s, "debug")) {
        *log_level = SC_LOG_LEVEL_DEBUG;
        return true;
    }

    if (!strcmp(s, "info")) {
        *log_level = SC_LOG_LEVEL_INFO;
        return true;
    }

    if (!strcmp(s, "warn")) {
        *log_level = SC_LOG_LEVEL_WARN;
        return true;
    }

    if (!strcmp(s, "error")) {
        *log_level = SC_LOG_LEVEL_ERROR;
        return true;
    }

    LOGE("Could not parse log level: %s", s);
    return false;
}

// item is a list of mod keys separated by '+' (e.g. "lctrl+lalt")
// returns a bitwise-or of SC_SHORTCUT_MOD_* constants (or 0 on error)
static unsigned
parse_shortcut_mods_item(const char *item, size_t len) {
    unsigned mod = 0;

    for (;;) {
        char *plus = strchr(item, '+');
        // strchr() does not consider the "len" parameter, to it could find an
        // occurrence too far in the string (there is no strnchr())
        bool has_plus = plus && plus < item + len;

        assert(!has_plus || plus > item);
        size_t key_len = has_plus ? (size_t) (plus - item) : len;

#define STREQ(literal, s, len) \
    ((sizeof(literal)-1 == len) && !memcmp(literal, s, len))

        if (STREQ("lctrl", item, key_len)) {
            mod |= SC_SHORTCUT_MOD_LCTRL;
        } else if (STREQ("rctrl", item, key_len)) {
            mod |= SC_SHORTCUT_MOD_RCTRL;
        } else if (STREQ("lalt", item, key_len)) {
            mod |= SC_SHORTCUT_MOD_LALT;
        } else if (STREQ("ralt", item, key_len)) {
            mod |= SC_SHORTCUT_MOD_RALT;
        } else if (STREQ("lsuper", item, key_len)) {
            mod |= SC_SHORTCUT_MOD_LSUPER;
        } else if (STREQ("rsuper", item, key_len)) {
            mod |= SC_SHORTCUT_MOD_RSUPER;
        } else {
            LOGE("Unknown modifier key: %.*s "
                 "(must be one of: lctrl, rctrl, lalt, ralt, lsuper, rsuper)",
                 (int) key_len, item);
            return 0;
        }
#undef STREQ

        if (!has_plus) {
            break;
        }

        item = plus + 1;
        assert(len >= key_len + 1);
        len -= key_len + 1;
    }

    return mod;
}

static bool
parse_shortcut_mods(const char *s, struct sc_shortcut_mods *mods) {
    unsigned count = 0;
    unsigned current = 0;

    // LCtrl+LAlt or RCtrl or LCtrl+RSuper: "lctrl+lalt,rctrl,lctrl+rsuper"

    for (;;) {
        char *comma = strchr(s, ',');
        if (comma && count == SC_MAX_SHORTCUT_MODS - 1) {
            assert(count < SC_MAX_SHORTCUT_MODS);
            LOGW("Too many shortcut modifiers alternatives");
            return false;
        }

        assert(!comma || comma > s);
        size_t limit = comma ? (size_t) (comma - s) : strlen(s);

        unsigned mod = parse_shortcut_mods_item(s, limit);
        if (!mod) {
            LOGE("Invalid modifier keys: %.*s", (int) limit, s);
            return false;
        }

        mods->data[current++] = mod;
        ++count;

        if (!comma) {
            break;
        }

        s = comma + 1;
    }

    mods->count = count;

    return true;
}

#ifdef SC_TEST
// expose the function to unit-tests
bool
sc_parse_shortcut_mods(const char *s, struct sc_shortcut_mods *mods) {
    return parse_shortcut_mods(s, mods);
}
#endif

static bool
parse_record_format(const char *optarg, enum sc_record_format *format) {
    if (!strcmp(optarg, "mp4")) {
        *format = SC_RECORD_FORMAT_MP4;
        return true;
    }
    if (!strcmp(optarg, "mkv")) {
        *format = SC_RECORD_FORMAT_MKV;
        return true;
    }
    LOGE("Unsupported format: %s (expected mp4 or mkv)", optarg);
    return false;
}

static bool
parse_ip(const char *optarg, uint32_t *ipv4) {
    return net_parse_ipv4(optarg, ipv4);
}

static bool
parse_port(const char *optarg, uint16_t *port) {
    long value;
    if (!parse_integer_arg(optarg, &value, false, 0, 0xFFFF, "port")) {
        return false;
    }
    *port = (uint16_t) value;
    return true;
}

static enum sc_record_format
guess_record_format(const char *filename) {
    size_t len = strlen(filename);
    if (len < 4) {
        return 0;
    }
    const char *ext = &filename[len - 4];
    if (!strcmp(ext, ".mp4")) {
        return SC_RECORD_FORMAT_MP4;
    }
    if (!strcmp(ext, ".mkv")) {
        return SC_RECORD_FORMAT_MKV;
    }
    return 0;
}

static bool
parse_args_with_getopt(struct scrcpy_cli_args *args, int argc, char *argv[],
                       const char *optstring, const struct option *longopts) {
    struct scrcpy_options *opts = &args->opts;

    optind = 0; // reset to start from the first argument in tests

    int c;
    while ((c = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
        switch (c) {
            case 'b':
                if (!parse_bit_rate(optarg, &opts->bit_rate)) {
                    return false;
                }
                break;
            case OPT_CROP:
                opts->crop = optarg;
                break;
            case OPT_DISPLAY_ID:
                if (!parse_display_id(optarg, &opts->display_id)) {
                    return false;
                }
                break;
            case 'd':
                opts->select_usb = true;
                break;
            case 'e':
                opts->select_tcpip = true;
                break;
            case 'f':
                opts->fullscreen = true;
                break;
            case 'F':
                LOGW("Deprecated option -F. Use --record-format instead.");
                // fall through
            case OPT_RECORD_FORMAT:
                if (!parse_record_format(optarg, &opts->record_format)) {
                    return false;
                }
                break;
            case 'h':
                args->help = true;
                break;
            case 'K':
#ifdef HAVE_USB
                opts->keyboard_input_mode = SC_KEYBOARD_INPUT_MODE_HID;
                break;
#else
                LOGE("HID over AOA (-K/--hid-keyboard) is disabled.");
                return false;
#endif
            case OPT_MAX_FPS:
                if (!parse_max_fps(optarg, &opts->max_fps)) {
                    return false;
                }
                break;
            case 'm':
                if (!parse_max_size(optarg, &opts->max_size)) {
                    return false;
                }
                break;
            case 'M':
#ifdef HAVE_USB
                opts->mouse_input_mode = SC_MOUSE_INPUT_MODE_HID;
                break;
#else
                LOGE("HID over AOA (-M/--hid-mouse) is disabled.");
                return false;
#endif
            case OPT_LOCK_VIDEO_ORIENTATION:
                if (!parse_lock_video_orientation(optarg,
                        &opts->lock_video_orientation)) {
                    return false;
                }
                break;
            case OPT_TUNNEL_HOST:
                if (!parse_ip(optarg, &opts->tunnel_host)) {
                    return false;
                }
                break;
            case OPT_TUNNEL_PORT:
                if (!parse_port(optarg, &opts->tunnel_port)) {
                    return false;
                }
                break;
            case 'n':
                opts->control = false;
                break;
            case 'N':
                opts->display = false;
                break;
            case 'p':
                if (!parse_port_range(optarg, &opts->port_range)) {
                    return false;
                }
                break;
            case 'r':
                opts->record_filename = optarg;
                break;
            case 's':
                opts->serial = optarg;
                break;
            case 'S':
                opts->turn_screen_off = true;
                break;
            case 't':
                opts->show_touches = true;
                break;
            case OPT_ALWAYS_ON_TOP:
                opts->always_on_top = true;
                break;
            case 'v':
                args->version = true;
                break;
            case 'V':
                if (!parse_log_level(optarg, &opts->log_level)) {
                    return false;
                }
                break;
            case 'w':
                opts->stay_awake = true;
                break;
            case OPT_RENDER_EXPIRED_FRAMES:
                LOGW("Option --render-expired-frames has been removed. This "
                     "flag has been ignored.");
                break;
            case OPT_WINDOW_TITLE:
                opts->window_title = optarg;
                break;
            case OPT_WINDOW_X:
                if (!parse_window_position(optarg, &opts->window_x)) {
                    return false;
                }
                break;
            case OPT_WINDOW_Y:
                if (!parse_window_position(optarg, &opts->window_y)) {
                    return false;
                }
                break;
            case OPT_WINDOW_WIDTH:
                if (!parse_window_dimension(optarg, &opts->window_width)) {
                    return false;
                }
                break;
            case OPT_WINDOW_HEIGHT:
                if (!parse_window_dimension(optarg, &opts->window_height)) {
                    return false;
                }
                break;
            case OPT_WINDOW_BORDERLESS:
                opts->window_borderless = true;
                break;
            case OPT_PUSH_TARGET:
                opts->push_target = optarg;
                break;
            case OPT_PREFER_TEXT:
                if (opts->key_inject_mode != SC_KEY_INJECT_MODE_MIXED) {
                    LOGE("--prefer-text is incompatible with --raw-key-events");
                    return false;
                }
                opts->key_inject_mode = SC_KEY_INJECT_MODE_TEXT;
                break;
            case OPT_RAW_KEY_EVENTS:
                if (opts->key_inject_mode != SC_KEY_INJECT_MODE_MIXED) {
                    LOGE("--prefer-text is incompatible with --raw-key-events");
                    return false;
                }
                opts->key_inject_mode = SC_KEY_INJECT_MODE_RAW;
                break;
            case OPT_ROTATION:
                if (!parse_rotation(optarg, &opts->rotation)) {
                    return false;
                }
                break;
            case OPT_RENDER_DRIVER:
                opts->render_driver = optarg;
                break;
            case OPT_NO_MIPMAPS:
                opts->mipmaps = false;
                break;
            case OPT_NO_KEY_REPEAT:
                opts->forward_key_repeat = false;
                break;
            case OPT_CODEC_OPTIONS:
                opts->codec_options = optarg;
                break;
            case OPT_ENCODER_NAME:
                opts->encoder_name = optarg;
                break;
            case OPT_FORCE_ADB_FORWARD:
                opts->force_adb_forward = true;
                break;
            case OPT_DISABLE_SCREENSAVER:
                opts->disable_screensaver = true;
                break;
            case OPT_SHORTCUT_MOD:
                if (!parse_shortcut_mods(optarg, &opts->shortcut_mods)) {
                    return false;
                }
                break;
            case OPT_FORWARD_ALL_CLICKS:
                opts->forward_all_clicks = true;
                break;
            case OPT_LEGACY_PASTE:
                opts->legacy_paste = true;
                break;
            case OPT_POWER_OFF_ON_CLOSE:
                opts->power_off_on_close = true;
                break;
            case OPT_DISPLAY_BUFFER:
                if (!parse_buffering_time(optarg, &opts->display_buffer)) {
                    return false;
                }
                break;
            case OPT_NO_CLIPBOARD_AUTOSYNC:
                opts->clipboard_autosync = false;
                break;
            case OPT_TCPIP:
                opts->tcpip = true;
                opts->tcpip_dst = optarg;
                break;
            case OPT_NO_DOWNSIZE_ON_ERROR:
                opts->downsize_on_error = false;
                break;
            case OPT_NO_CLEANUP:
                opts->cleanup = false;
                break;
            case OPT_NO_POWER_ON:
                opts->power_on = false;
                break;
            case OPT_PRINT_FPS:
                opts->start_fps_counter = true;
                break;
            case OPT_OTG:
#ifdef HAVE_USB
                opts->otg = true;
                break;
#else
                LOGE("OTG mode (--otg) is disabled.");
                return false;
#endif
            case OPT_V4L2_SINK:
#ifdef HAVE_V4L2
                opts->v4l2_device = optarg;
                break;
#else
                LOGE("V4L2 (--v4l2-sink) is disabled (or unsupported on this "
                     "platform).");
                return false;
#endif
            case OPT_V4L2_BUFFER:
#ifdef HAVE_V4L2
                if (!parse_buffering_time(optarg, &opts->v4l2_buffer)) {
                    return false;
                }
                break;
#else
                LOGE("V4L2 (--v4l2-buffer) is only available on Linux.");
                return false;
#endif
            default:
                // getopt prints the error message on stderr
                return false;
        }
    }

    int index = optind;
    if (index < argc) {
        LOGE("Unexpected additional argument: %s", argv[index]);
        return false;
    }

    // If a TCP/IP address is provided, then tcpip must be enabled
    assert(opts->tcpip || !opts->tcpip_dst);

    unsigned selectors = !!opts->serial
                       + !!opts->tcpip_dst
                       + opts->select_tcpip
                       + opts->select_usb;
    if (selectors > 1) {
        LOGE("At most one device selector option may be passed, among:\n"
             "  --serial (-s)\n"
             "  --select-usb (-d)\n"
             "  --select-tcpip (-e)\n"
             "  --tcpip=<addr> (with an argument)");
        return false;
    }

#ifdef HAVE_V4L2
    if (!opts->display && !opts->record_filename && !opts->v4l2_device) {
        LOGE("-N/--no-display requires either screen recording (-r/--record)"
             " or sink to v4l2loopback device (--v4l2-sink)");
        return false;
    }

    if (opts->v4l2_device) {
        if (opts->lock_video_orientation ==
                SC_LOCK_VIDEO_ORIENTATION_UNLOCKED) {
            LOGI("Video orientation is locked for v4l2 sink. "
                 "See --lock-video-orientation.");
            opts->lock_video_orientation = SC_LOCK_VIDEO_ORIENTATION_INITIAL;
        }

        // V4L2 could not handle size change.
        // Do not log because downsizing on error is the default behavior,
        // not an explicit request from the user.
        opts->downsize_on_error = false;
    }

    if (opts->v4l2_buffer && !opts->v4l2_device) {
        LOGE("V4L2 buffer value without V4L2 sink\n");
        return false;
    }
#else
    if (!opts->display && !opts->record_filename) {
        LOGE("-N/--no-display requires screen recording (-r/--record)");
        return false;
    }
#endif

    if ((opts->tunnel_host || opts->tunnel_port) && !opts->force_adb_forward) {
        LOGI("Tunnel host/port is set, "
             "--force-adb-forward automatically enabled.");
        opts->force_adb_forward = true;
    }

    if (opts->record_format && !opts->record_filename) {
        LOGE("Record format specified without recording");
        return false;
    }

    if (opts->record_filename && !opts->record_format) {
        opts->record_format = guess_record_format(opts->record_filename);
        if (!opts->record_format) {
            LOGE("No format specified for \"%s\" "
                 "(try with --record-format=mkv)",
                 opts->record_filename);
            return false;
        }
    }

    if (!opts->control) {
        if (opts->turn_screen_off) {
            LOGE("Could not request to turn screen off if control is disabled");
            return false;
        }
        if (opts->stay_awake) {
            LOGE("Could not request to stay awake if control is disabled");
            return false;
        }
        if (opts->show_touches) {
            LOGE("Could not request to show touches if control is disabled");
            return false;
        }
        if (opts->power_off_on_close) {
            LOGE("Could not request power off on close if control is disabled");
            return false;
        }
    }

#ifdef HAVE_USB

# ifdef _WIN32
    if (!opts->otg && (opts->keyboard_input_mode == SC_KEYBOARD_INPUT_MODE_HID
                    || opts->mouse_input_mode == SC_MOUSE_INPUT_MODE_HID)) {
        LOGE("On Windows, it is not possible to open a USB device already open "
             "by another process (like adb).");
        LOGE("Therefore, -K/--hid-keyboard and -M/--hid-mouse may only work in "
             "OTG mode (--otg).");
        return false;
    }
# endif

    if (opts->otg) {
        // OTG mode is compatible with only very few options.
        // Only report obvious errors.
        if (opts->record_filename) {
            LOGE("OTG mode: could not record");
            return false;
        }
        if (opts->turn_screen_off) {
            LOGE("OTG mode: could not turn screen off");
            return false;
        }
        if (opts->stay_awake) {
            LOGE("OTG mode: could not stay awake");
            return false;
        }
        if (opts->show_touches) {
            LOGE("OTG mode: could not request to show touches");
            return false;
        }
        if (opts->power_off_on_close) {
            LOGE("OTG mode: could not request power off on close");
            return false;
        }
        if (opts->display_id) {
            LOGE("OTG mode: could not select display");
            return false;
        }
# ifdef HAVE_V4L2
        if (opts->v4l2_device) {
            LOGE("OTG mode: could not sink to V4L2 device");
            return false;
        }
# endif
    }
#endif

    return true;
}

bool
scrcpy_parse_args(struct scrcpy_cli_args *args, int argc, char *argv[]) {
    struct sc_getopt_adapter adapter;
    if (!sc_getopt_adapter_init(&adapter)) {
        LOGW("Could not create getopt adapter");
        return false;
    }

    bool ret = parse_args_with_getopt(args, argc, argv, adapter.optstring,
                                      adapter.longopts);

    sc_getopt_adapter_destroy(&adapter);

    return ret;
}
