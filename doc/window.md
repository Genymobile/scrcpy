# Window

## Disable window

To disable window (may be useful for recording or for playing audio only):

```bash
scrcpy --no-window --record=file.mp4
# Ctrl+C to interrupt
```

## Title

By default, the window title is the device model. It can be changed:

```bash
scrcpy --window-title='My device'
```

## Position and size

The initial window position and size may be specified:

```bash
scrcpy --window-x=100 --window-y=100 --window-width=800 --window-height=600
```

By default, the window aspect ratio is preserved when resizing. To disable this
behavior:

```bash
scrcpy --no-window-aspect-ratio-lock
```

## Background color

To maintain the device aspect ratio (when using `--no-window-aspect-ratio-lock`
or in fullscreen mode), padding is added around the device. By default, it is
dark gray (`#222`).

This can be changed:

```bash
# Use the hexadecimal color code #234567
# (r, g, b) = (0x23, 0x45, 0x67) = (35, 69, 103)
scrcpy --fullscreen --background-color=#234567

# The leading '#' is optional
scrcpy --fullscreen --background-color=234567

# Hexadecimal color shorthand (3-digit form) is supported
scrcpy --fullscreen --background-color=#567
# is equivalent to:
scrcpy --fullscreen --background-color=#556677
```

## Borderless

To disable window decorations:

```bash
scrcpy --window-borderless
```

## Always on top

To keep the window always on top:

```bash
scrcpy --always-on-top
```

## Fullscreen

The app may be started directly in fullscreen:

```bash
scrcpy --fullscreen
scrcpy -f   # short version
```

Fullscreen mode can then be toggled dynamically with <kbd>MOD</kbd>+<kbd>f</kbd>
or <kbd>F11</kbd> (see [shortcuts](shortcuts.md)).


## Disable screensaver

By default, _scrcpy_ does not prevent the screensaver from running on the
computer. To disable it:

```bash
scrcpy --disable-screensaver
```


## Render fit

By default, the video stream is rendered in [letterbox] mode: the content fits
the window as best as possible while preserving the aspect ratio.

For [flex displays], the display is continuously resized to match the window, so
render fit is _unscaled_: the content is rendered without scaling.

It is also possible to _stretch_ the content to fit the window without
preserving the aspect ratio (`--render-fit=stretched`).

```bash
scrcpy --render-fit=letterbox  # default
scrcpy --render-fit=unscaled   # default for flex displays
scrcpy --render-fit=stretched
```

[letterbox]: https://en.wikipedia.org/wiki/Letterboxing_(filming)
[flex displays]: virtual-display.md#flex-display
