# OTG

By default, _scrcpy_ injects input events at the Android API level. As an
alternative, when connected over USB, it is possible to send HID events, so that
scrcpy behaves as if it was a physical keyboard and/or mouse connected to the
Android device.

A special mode allows to control the device without mirroring, using AOA
[keyboard](keyboard.md#aoa) and [mouse](mouse.md#aoa). Therefore, it is possible
to run _scrcpy_ with only physical keyboard and mouse simulation (HID), as if
the computer keyboard and mouse were plugged directly to the device via an OTG
cable.

In this mode, `adb` (USB debugging) is not necessary, and mirroring is disabled.

This is similar to `--keyboard=aoa --mouse=aoa`, but without mirroring.

To enable OTG mode:

```bash
scrcpy --otg
# Pass the serial if several USB devices are available
scrcpy --otg -s 0123456789abcdef
```

It is possible to disable HID keyboard or HID mouse:

```bash
scrcpy --otg --keyboard=disabled
scrcpy --otg --mouse=disabled
```

It only works if the device is connected over USB.

## OTG issues on Windows

See [FAQ](/FAQ.md#otg-issues-on-windows).
