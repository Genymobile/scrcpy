# Mouse

Several mouse input modes are available:

 - `--mouse=sdk` (default)
 - `--mouse=uhid` (or `-M`): simulates a physical HID mouse using the UHID
   kernel module on the device
 - `--mouse=aoa`: simulates a physical HID mouse using the AOAv2 protocol
 - `--mouse=disabled`


## SDK mouse

In this mode (`--mouse=sdk`, or if the parameter is omitted), mouse input events
are injected at the Android API level with absolute coordinates.

Note that on some devices, an additional option must be enabled in developer
options for this mouse mode to work. See
[prerequisites](/README.md#prerequisites).

### Mouse hover

By default, mouse hover (mouse motion without any clicks) events are forwarded
to the device. This can be disabled with:

```
scrcpy --no-mouse-hover
```

## Physical mouse simulation

Two modes allow to simulate a physical HID mouse on the device.

In these modes, the computer mouse is "captured": the mouse pointer disappears
from the computer and appears on the Android device instead.

The [shortcut mod](shortcuts.md) (either <kbd>Alt</kbd> or <kbd>Super</kbd> by
default) toggle (disable or enable) the mouse capture. Use one of them to give
the control of the mouse back to the computer.


### UHID

This mode simulates a physical HID mouse using the [UHID] kernel module on the
device.

[UHID]: https://kernel.org/doc/Documentation/hid/uhid.txt

To enable UHID mouse, use:

```bash
scrcpy --mouse=uhid
scrcpy -M  # short version
```

Note: UHID may not work on old Android versions due to permission errors.


### AOA

This mode simulates a physical HID mouse using the [AOAv2] protocol.

[AOAv2]: https://source.android.com/devices/accessories/aoa2#hid-support

To enable AOA mouse, use:

```bash
scrcpy --mouse=aoa
```

Contrary to the other modes, it works at the USB level directly (so it only
works over USB).

It does not use the scrcpy server, and does not require `adb` (USB debugging).
Therefore, it is possible to control the device (but not mirror) even with USB
debugging disabled (see [OTG](otg.md)).

Note: On Windows, it may only work in [OTG mode](otg.md), not while mirroring
(it is not possible to open a USB device if it is already open by another
process like the _adb daemon_).


## Mouse bindings

By default, with SDK mouse:
 - right-click triggers `BACK` (or `POWER` on)
 - middle-click triggers `HOME`
 - the 4th click triggers `APP_SWITCH`
 - the 5th click expands the notification panel

The secondary clicks may be forwarded to the device instead by pressing the
<kbd>Shift</kbd> key (e.g. <kbd>Shift</kbd>+right-click injects a right click to
the device).

In AOA and UHID mouse modes, the default bindings are reversed: all clicks are
forwarded by default, and pressing <kbd>Shift</kbd> gives access to the
shortcuts (since the cursor is handled on the device side, it makes more sense
to forward all mouse buttons by default in these modes).

The shortcuts can be configured using `--mouse-bind=xxxx:xxxx` for any mouse
mode. The argument must be one or two sequences (separated by `:`) of exactly 4
characters, one for each secondary click:

```
                  .---- Shift + right click
       SECONDARY  |.--- Shift + middle click
        BINDINGS  ||.-- Shift + 4th click
                  |||.- Shift + 5th click
                  ||||
                  vvvv
--mouse-bind=xxxx:xxxx
             ^^^^
             ||||
   PRIMARY   ||| `- 5th click
  BINDINGS   || `-- 4th click
             | `--- middle click
              `---- right click
```

Each character must be one of the following:

 - `+`: forward the click to the device
 - `-`: ignore the click
 - `b`: trigger shortcut `BACK` (or turn screen on if off)
 - `h`: trigger shortcut `HOME`
 - `s`: trigger shortcut `APP_SWITCH`
 - `n`: trigger shortcut "expand notification panel"

For example:

```bash
scrcpy --mouse-bind=bhsn:++++  # the default mode for SDK mouse
scrcpy --mouse-bind=++++:bhsn  # the default mode for AOA and UHID
scrcpy --mouse-bind=++bh:++sn  # forward right and middle clicks,
                               # use 4th and 5th for BACK and HOME,
                               # use Shift+4th and Shift+5th for APP_SWITCH
                               # and expand notification panel
```

The second sequence of bindings may be omitted. In that case, it is the same as
the first one:

```bash
scrcpy --mouse-bind=bhsn
scrcpy --mouse-bind=bhsn:bhsn  # equivalent
```
