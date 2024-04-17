# Keyboard

Several keyboard input modes are available:

 - `--keyboard=sdk` (default)
 - `--keyboard=uhid` (or `-K`): simulates a physical HID keyboard using the UHID
   kernel module on the device
 - `--keyboard=aoa`: simulates a physical HID keyboard using the AOAv2 protocol
 - `--keyboard=disabled`

By default, `sdk` is used, but if you use scrcpy regularly, it is recommended to
use [`uhid`](#uhid) and configure the keyboard layout once and for all.


## SDK keyboard

In this mode (`--keyboard=sdk`, or if the parameter is omitted), keyboard input
events are injected at the Android API level. It works everywhere, but it is
limited to ASCII and some other characters.

Note that on some devices, an additional option must be enabled in developer
options for this keyboard mode to work. See
[prerequisites](/README.md#prerequisites).

Additional parameters (specific to `--keyboard=sdk`) described below allow to
customize the behavior.


### Text injection preference

Two kinds of [events][textevents] are generated when typing text:
 - _key events_, signaling that a key is pressed or released;
 - _text events_, signaling that a text has been entered.

By default, numbers and "special characters" are inserted using text events, but
letters are injected using key events, so that the keyboard behaves as expected
in games (typically for WASD keys).

But this may [cause issues][prefertext]. If you encounter such a problem, you
can inject letters as text (or just switch to [UHID](#uhid)):

```bash
scrcpy --prefer-text
```

(but this will break keyboard behavior in games)

On the contrary, you could force to always inject raw key events:

```bash
scrcpy --raw-key-events
```

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


### Key repeat

By default, holding a key down generates repeated key events. Ths can cause
performance problems in some games, where these events are useless anyway.

To avoid forwarding repeated key events:

```bash
scrcpy --no-key-repeat
```


## Physical keyboard simulation

Two modes allow to simulate a physical HID keyboard on the device.

To work properly, it is necessary to configure (once and for all) the keyboard
layout on the device to match that of the computer.

The configuration page can be opened in one of the following ways:
 - from the scrcpy window (when `uhid` or `aoa` is used), by pressing
   <kbd>MOD</kbd>+<kbd>k</kbd> (see [shortcuts](shortcuts.md))
 - from the device, in Settings → System → Languages and input → Physical
   devices
 - from a terminal on the computer, by executing `adb shell am start -a
   android.settings.HARD_KEYBOARD_SETTINGS`

From this configuration page, it is also possible to enable or disable on-screen
keyboard.


### UHID

This mode simulates a physical HID keyboard using the [UHID] kernel module on the
device.

[UHID]: https://kernel.org/doc/Documentation/hid/uhid.txt

To enable UHID keyboard, use:

```bash
scrcpy --keyboard=uhid
scrcpy -K  # short version
```

Once the keyboard layout is configured (see above), it is the best mode for
using the keyboard while mirroring:

 - it works for all characters and IME (contrary to `--keyboard=sdk`)
 - the on-screen keyboard can be disabled (contrary to `--keyboard=sdk`)
 - it works over TCP/IP (wirelessly) (contrary to `--keyboard=aoa`)
 - there are no issues on Windows (contrary to `--keyboard=aoa`)

One drawback is that it may not work on old Android versions due to permission
errors.


### AOA

This mode simulates a physical HID keyboard using the [AOAv2] protocol.

[AOAv2]: https://source.android.com/devices/accessories/aoa2#hid-support

To enable AOA keyboard, use:

```bash
scrcpy --keyboard=aoa
```

Contrary to the other modes, it works at the USB level directly (so it only
works over USB).

It does not use the scrcpy server, and does not require `adb` (USB debugging).
Therefore, it is possible to control the device (but not mirror) even with USB
debugging disabled (see [OTG](otg.md)).

Note: On Windows, it may only work in [OTG mode](otg.md), not while mirroring
(it is not possible to open a USB device if it is already open by another
process like the _adb daemon_).
