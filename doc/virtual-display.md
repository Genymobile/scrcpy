# Virtual display

## New display

To mirror a new virtual display instead of the device screen:

```bash
scrcpy --new-display=1920x1080
scrcpy --new-display=1920x1080/420  # force 420 dpi
scrcpy --new-display         # use the main display size and density
scrcpy --new-display=/240    # use the main display size and 240 dpi
```

The new virtual display is destroyed on exit.


## Flex display

To continuously resize the virtual display to match the window size, enable
"flex display" using `--flex-display` (or `-x`):

```bash
# Start Android Settings in a window
scrcpy --new-display=1024x768 --start-app=com.android.settings --flex-display

# -x is equivalent to --flex-display
scrcpy --new-display=1024x768 --start-app=com.android.settings -x

# By default, the display size/dpi is 1280x960/160
scrcpy --new-display --start-app=com.android.settings --flex-display
```

Use [`--keep-active`][keep-active] to prevent the screen from turning off:


```bash
scrcpy --new-display --flex-display --keep-active
```

[keep-active]: device.md#keep-active

Increase the [bit rate] (and/or change the [codec]) to maintain good quality
even with large windows:

```bash
scrcpy --new-display -x --video-codec=h265 -b16M
```

[bit rate]: video.md#bit-rate
[codec]: video.md#codec


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


## Android 17 desktop compatibility

On Android 17, the native launcher may use the desktop launch path for a virtual
display. scrcpy requests content-mode switching for displays with system
decorations, but Android also requires freeform window support. Without it, the
launcher may react to navigation while silently ignoring app icons.

On devices missing that capability, explicitly opt in to temporarily enabling it:

```bash
scrcpy --new-display --vd-enable-freeform
```

This option changes the **device-wide** `enable_freeform_support` setting and may
reconfigure the main display. It is disabled by default. It requires Android 17
or newer, a new display with video and system decorations, and cleanup enabled.
Only one session using this option may run on a device at a time. Other sessions
and apps on the device also observe the temporary setting.

With this option, scrcpy waits for WindowManager to report desktop
support on the default display before creating the virtual display, then checks
the new display's eligibility. An
unsupported API, policy restriction or failure to become eligible aborts startup
and triggers restoration; setting the value alone is not treated as success.
These checks do not guarantee that every device's launcher supports desktop
launching.

A separate guardian process saves the original value before changing it. After
the session ends, it restores that value (or deletes the key if originally
absent), unless another writer has changed it to a different value. A later
external write of the same value cannot be distinguished. On normal exit, the
virtual display is released before restoration. The guardian also handles loss
of the server process, but **cannot restore after it is itself killed or the
device restarts**. In that case, restore the original setting manually; record
its value before running experiments with this option:

```bash
adb shell settings get global enable_freeform_support
# If the key was originally absent:
adb shell settings delete global enable_freeform_support
# Otherwise, restore the recorded original value:
adb shell settings put global enable_freeform_support ORIGINAL_VALUE
```

This is a compatibility workaround for the desktop launch prerequisites. It does
not change Android's launcher fallback behavior, force apps to open fullscreen,
or isolate app tasks, focus or keyboards from the main display. In particular,
the native launcher may restore an existing recent task on the main display
even when the virtual display is desktop-eligible.


## System decorations

By default, virtual display system decorations are enabled. To disable them, use
`--no-vd-system-decorations`:

```bash
scrcpy --new-display --no-vd-system-decorations
```

This is useful for some devices which might display a broken UI, or to disable
any default launcher UI available in virtual displays.

Note that if no app is started, no content will be rendered, so no video frame
will be produced at all.


## Destroy on close

By default, when the virtual display is closed, the running apps are destroyed.

To move them to the main display instead, use:

```bash
scrcpy --new-display --no-vd-destroy-content
```


## Display IME policy

By default, the virtual display IME appears on the default display.

To make it appear on the local display, use `--display-ime-policy=local`:

```bash
scrcpy --display-id=1 --display-ime-policy=local
scrcpy --new-display --display-ime-policy=local
```
