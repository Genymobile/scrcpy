# OTG

By default, _scrcpy_ injects input events at the Android API level. As an
alternative, it is possible to send HID events, so that scrcpy behaves as if it
was a [physical keyboard] and/or a [physical mouse] connected to the Android
device (see [keyboard](keyboard.md) and [mouse](mouse.md)).

[physical keyboard]: keyboard.md#physical-keyboard-simulation
[physical mouse]: physical-keyboard-simulation

A special mode (OTG) allows to control the device using AOA
[keyboard](keyboard.md#aoa) and [mouse](mouse.md#aoa), without using _adb_ at
all (so USB debugging is not necessary). In this mode, video and audio are
disabled, and `--keyboard=aoa and `--mouse=aoa` are implicitly set.

Therefore, it is possible to run _scrcpy_ with only physical keyboard and mouse
simulation, as if the computer keyboard and mouse were plugged directly to the
device via an OTG cable.

To enable OTG mode:

```bash
scrcpy --otg
# Pass the serial if several USB devices are available
scrcpy --otg -s 0123456789abcdef
```

It is possible to disable keyboard or mouse:

```bash
scrcpy --otg --keyboard=disabled
scrcpy --otg --mouse=disabled
```

It only works if the device is connected over USB.

## OTG issues on Windows

See [FAQ](/FAQ.md#otg-issues-on-windows).


## Control only

Note that the purpose of OTG is to control the device without USB debugging
(adb).

If you want to solely control the device without mirroring while USB debugging
is enabled, then OTG mode is not necessary.

Instead, disable video and audio, and select UHID (or AOA):

```bash
scrcpy --no-video --no-audio --keyboard=uhid --mouse=uhid
scrcpy --no-video --no-audio -KM  # short version
scrcpy --no-video --no-audio --keyboard=aoa --mouse=aoa
```

One benefit of UHID is that it also works wirelessly.
