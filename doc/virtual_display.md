# Virtual display

## New display

To mirror a new virtual display instead of the device screen:

```bash
scrcpy --new-display=1920x1080
scrcpy --new-display=1920x1080/420  # force 420 dpi
scrcpy --new-display         # use the main display size and density
scrcpy --new-display=/240    # use the main display size and 240 dpi
```

You can make the display resizable by adding `:r` to the option. Additionally, you can specify a resolution factor after `:r` to scale the display size:

```bash
scrcpy --new-display=1920x1080:r           # resizable display
scrcpy --new-display=1920x1080:r2.0        # resizable with 2x factor
scrcpy --new-display=:r0.5                 # resizable with 0.5x factor
scrcpy --new-display=/420:r1.5             # resizable with 1.5x factor
```

The resolution factor must be between 0.1 and 10.0.

The new virtual display is destroyed on exit.

## Start app

On some devices, a launcher is available in the virtual display.

When no launcher is available (or if is explicitly disabled by
[`--no-vd-system-decorations`](#system-decorations)), the virtual display is
empty. In that case, you must [start an Android
app](device.md#start-android-app).

For example:

```bash
scrcpy --new-display=1920x1080 --start-app=org.videolan.vlc
```

The app may itself be a launcher. For example, to run the open source [Fossify
Launcher]:

```bash
scrcpy --new-display=1920x1080 --no-vd-system-decorations --start-app=org.fossify.home
```

[Fossify Launcher]: https://f-droid.org/en/packages/org.fossify.home/


## System decorations

By default, virtual display system decorations are enabled. To disable them, use
`--no-vd-system-decorations`:

```
scrcpy --new-display --no-vd-system-decorations
```

This is useful for some devices which might display a broken UI, or to disable
any default launcher UI available in virtual displays.

Note that if no app is started, no content will be rendered, so no video frame
will be produced at all.


## Destroy on close

By default, when the virtual display is closed, the running apps are destroyed.

To move them to the main display instead, use:

```
scrcpy --new-display --no-vd-destroy-content
```


## Display IME policy

By default, the virtual display IME appears on the default display.

To make it appear on the local display, use `--display-ime-policy=local`:

```bash
scrcpy --display-id=1 --display-ime-policy=local
scrcpy --new-display --display-ime-policy=local
```
