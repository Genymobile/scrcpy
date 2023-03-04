# Control

## Read-only

To disable controls (everything which can interact with the device: input keys,
mouse events, drag&drop files):

```bash
scrcpy --no-control
scrcpy -n   # short version
```


## Text injection preference

Two kinds of [events][textevents] are generated when typing text:
 - _key events_, signaling that a key is pressed or released;
 - _text events_, signaling that a text has been entered.

By default, letters are injected using key events, so that the keyboard behaves
as expected in games (typically for WASD keys).

But this may [cause issues][prefertext]. If you encounter such a problem, you
can avoid it by:

```bash
scrcpy --prefer-text
```

(but this will break keyboard behavior in games)

On the contrary, you could force to always inject raw key events:

```bash
scrcpy --raw-key-events
```

These options have no effect on HID keyboard (all key events are sent as
scancodes in this mode).

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


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

## Pinch-to-zoom

To simulate "pinch-to-zoom": <kbd>Ctrl</kbd>+_click-and-move_.

More precisely, hold down <kbd>Ctrl</kbd> while pressing the left-click button.
Until the left-click button is released, all mouse movements scale and rotate
the content (if supported by the app) relative to the center of the screen.

Technically, _scrcpy_ generates additional touch events from a "virtual finger"
at a location inverted through the center of the screen.


## Key repeat

By default, holding a key down generates repeated key events. This can cause
performance problems in some games, where these events are useless anyway.

To avoid forwarding repeated key events:

```bash
scrcpy --no-key-repeat
```

This option has no effect on HID keyboard (key repeat is handled by Android
directly in this mode).


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

## Physical keyboard and mouse simulation

See the dedicated [HID/OTG](hid-otg.md) page.
