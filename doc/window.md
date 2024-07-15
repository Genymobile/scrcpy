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
