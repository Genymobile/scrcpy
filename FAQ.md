# Frequently Asked Questions

## Common issues

The application is very young, it is not unlikely that you encounter problems
with it.

Here are the common reported problems and their status.


### On Windows, my device is not detected

The most common is your device not being detected by `adb`, or is unauthorized.
Check everything is ok by calling:

    adb devices

Windows may need some [drivers] to detect your device.

[drivers]: https://developer.android.com/studio/run/oem-usb.html

If you still encounter problems, please see [issue 9].

[issue 9]: https://github.com/Genymobile/scrcpy/issues/9


### Mouse clicks do not work

On some devices, you may need to enable an option to allow [simulating input].

[simulating input]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


### Mouse clicks at wrong location

On MacOS, with HiDPI support and multiple screens, input location are wrongly
scaled. See [issue 15].

[issue 15]: https://github.com/Genymobile/scrcpy/issues/15

A workaround is to build with HiDPI support disabled:

```bash
meson x --buildtype release -Dhidpi_support=false
```

However, the video will be displayed at lower resolution.


### KWin compositor crashes

On Plasma Desktop, compositor is disabled while _scrcpy_ is running.

As a workaround, [disable "Block compositing"][kwin].

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613
