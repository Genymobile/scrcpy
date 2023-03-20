# HID/OTG

By default, _scrcpy_ injects input events at the Android API level. As an
alternative, when connected over USB, it is possible to send HID events, so that
scrcpy behaves as if it was a physical keyboard and/or mouse connected to the
Android device.

A special [OTG](#otg) mode allows to control the device without mirroring (and
without USB debugging).


## Physical keyboard simulation

By default, _scrcpy_ uses Android key or text injection. It works everywhere,
but is limited to ASCII.

Instead, it can simulate a physical USB keyboard on Android to provide a better
input experience (using [USB HID over AOAv2][hid-aoav2]): the virtual keyboard
is disabled and it works for all characters and IME.

[hid-aoav2]: https://source.android.com/devices/accessories/aoa2#hid-support

However, it only works if the device is connected via USB.

Note: On Windows, it may only work in [OTG mode](#otg), not while mirroring (it
is not possible to open a USB device if it is already open by another process
like the _adb daemon_).

To enable this mode:

```bash
scrcpy --hid-keyboard
scrcpy -K   # short version
```

If it fails for some reason (for example because the device is not connected via
USB), it automatically fallbacks to the default mode (with a log in the
console). This allows using the same command line options when connected over
USB and TCP/IP.

In this mode, raw key events (scancodes) are sent to the device, independently
of the host key mapping. Therefore, if your keyboard layout does not match, it
must be configured on the Android device, in Settings → System → Languages and
input → [Physical keyboard].

This settings page can be started directly:

```bash
adb shell am start -a android.settings.HARD_KEYBOARD_SETTINGS
```

However, the option is only available when the HID keyboard is enabled (or when
a physical keyboard is connected).

[Physical keyboard]: https://github.com/Genymobile/scrcpy/pull/2632#issuecomment-923756915


## Physical mouse simulation

By default, _scrcpy_ uses Android mouse events injection with absolute
coordinates. By simulating a physical mouse, a mouse pointer appears on the
Android device, and relative mouse motion, clicks and scrolls are injected.

To enable this mode:

```bash
scrcpy --hid-mouse
scrcpy -M   # short version
```

When this mode is enabled, the computer mouse is "captured" (the mouse pointer
disappears from the computer and appears on the Android device instead).

Special capture keys, either <kbd>Alt</kbd> or <kbd>Super</kbd>, toggle
(disable or enable) the mouse capture. Use one of them to give the control of
the mouse back to the computer.


## OTG

It is possible to run _scrcpy_ with only physical keyboard and mouse simulation
(HID), as if the computer keyboard and mouse were plugged directly to the device
via an OTG cable.

In this mode, `adb` (USB debugging) is not necessary, and mirroring is disabled.

This is similar to `--hid-keyboard --hid-mouse`, but without mirroring.

To enable OTG mode:

```bash
scrcpy --otg
# Pass the serial if several USB devices are available
scrcpy --otg -s 0123456789abcdef
```

It is possible to enable only HID keyboard or HID mouse:

```bash
scrcpy --otg --hid-keyboard              # keyboard only
scrcpy --otg --hid-mouse                 # mouse only
scrcpy --otg --hid-keyboard --hid-mouse  # keyboard and mouse
# for convenience, enable both by default
scrcpy --otg                             # keyboard and mouse
```

Like `--hid-keyboard` and `--hid-mouse`, it only works if the device is
connected over USB.

## HID/OTG issues on Windows

See [FAQ](/FAQ.md#hidotg-issues-on-windows).
