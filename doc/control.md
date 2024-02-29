# Control

## Read-only

To disable controls (everything which can interact with the device: input keys,
mouse events, drag&drop files):

```bash
scrcpy --no-control
scrcpy -n   # short version
```

## Keyboard and mouse

Read [keyboard](keyboard.md) and [mouse](mouse.md).


## Copy-paste

Any time the Android clipboard changes, it is automatically synchronized to the
computer clipboard.

Any <kbd>Ctrl</kbd> shortcut is forwarded to the device. In particular:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> typically copies
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> typically cuts
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> typically pastes (after computer-to-device
   clipboard synchronization)

This typically works as you expect.

The actual behavior depends on the active application though. For example,
_Termux_ sends SIGINT on <kbd>Ctrl</kbd>+<kbd>c</kbd> instead, and _K-9 Mail_
composes a new message.

To copy, cut and paste in such cases (but only supported on Android >= 7):
 - <kbd>MOD</kbd>+<kbd>c</kbd> injects `COPY`
 - <kbd>MOD</kbd>+<kbd>x</kbd> injects `CUT`
 - <kbd>MOD</kbd>+<kbd>v</kbd> injects `PASTE` (after computer-to-device
   clipboard synchronization)

In addition, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> injects the computer
clipboard text as a sequence of key events. This is useful when the component
does not accept text pasting (for example in _Termux_), but it can break
non-ASCII content.

**WARNING:** Pasting the computer clipboard to the device (either via
<kbd>Ctrl</kbd>+<kbd>v</kbd> or <kbd>MOD</kbd>+<kbd>v</kbd>) copies the content
into the Android clipboard. As a consequence, any Android application could read
its content. You should avoid pasting sensitive content (like passwords) that
way.

Some Android devices do not behave as expected when setting the device clipboard
programmatically. An option `--legacy-paste` is provided to change the behavior
of <kbd>Ctrl</kbd>+<kbd>v</kbd> and <kbd>MOD</kbd>+<kbd>v</kbd> so that they
also inject the computer clipboard text as a sequence of key events (the same
way as <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>).

To disable automatic clipboard synchronization, use
`--no-clipboard-autosync`.


## Pinch-to-zoom, rotate and tilt simulation

To simulate "pinch-to-zoom": <kbd>Ctrl</kbd>+_click-and-move_.

More precisely, hold down <kbd>Ctrl</kbd> while pressing the left-click button.
Until the left-click button is released, all mouse movements scale and rotate
the content (if supported by the app) relative to the center of the screen.

To simulate a tilt gesture: <kbd>Shift</kbd>+_click-and-move-up-or-down_.

Technically, _scrcpy_ generates additional touch events from a "virtual finger"
at a location inverted through the center of the screen. When pressing
<kbd>Ctrl</kbd> the x and y coordinates are inverted. Using <kbd>Shift</kbd>
only inverts x.

This only works for the default mouse mode (`--mouse=sdk`).


## Right-click and middle-click

By default, right-click triggers BACK (or POWER on) and middle-click triggers
HOME. To disable these shortcuts and forward the clicks to the device instead:

```bash
scrcpy --forward-all-clicks
```

## File drop

### Install APK

To install an APK, drag & drop an APK file (ending with `.apk`) to the _scrcpy_
window.

There is no visual feedback, a log is printed to the console.


### Push file to device

To push a file to `/sdcard/Download/` on the device, drag & drop a (non-APK)
file to the _scrcpy_ window.

There is no visual feedback, a log is printed to the console.

The target directory can be changed on start:

```bash
scrcpy --push-target=/sdcard/Movies/
```
