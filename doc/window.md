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
(see [shortcuts](shortcuts.md)).


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
render fit is _disabled_: the content is rendered at the top-left corner,
without scaling.

It is also possible to _stretch_ the content to fit the window without
preserving aspect ratio (`--render-fit=stretched`).

```bash
scrcpy --render-fit=letterbox  # default
scrcpy --render-fit=disabled   # default for flex displays
scrcpy --render-fit=stretched
```

[letterbox]: https://en.wikipedia.org/wiki/Letterboxing_(filming)
[flex displays]: virtual_display.md#flex-display
