# Virtual display

## New display

To mirror a new virtual display instead of the device screen:

```bash
scrcpy --new-display=1920x1080
scrcpy --new-display=1920x1080/420  # force 420 dpi
scrcpy --new-display         # use the main display size and density
scrcpy --new-display=/240    # use the main display size and 240 dpi
```

## Start app

On some devices, a launcher is available in the virtual display.

When no launcher is available, the virtual display is empty. In that case, you
must [start an Android app](device.md#start-android-app).

For example:

```bash
scrcpy --new-display=1920x1080 --start-app=org.videolan.vlc
```

## System decorations

By default, virtual display system decorations are enabled. But some devices
might display a broken UI;

Use `--no-vd-system-decorations` to disable it.

Note that if no app is started, no content will be rendered, so no video frame
will be produced at all.
