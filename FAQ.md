# Frequently Asked Questions

## Common issues

The application is very young, it is not unlikely that you encounter problems
with it.

Here are the common reported problems and their status.

### On Windows, I have no output in the console

When run in `cmd.exe`, the application does not print anything. Even `scrcpy
--help` have no output. We don't know why yet.

However, if you run the very same `scrcpy.exe` from
[MSYS2](https://www.msys2.org/) (`mingw64`), then it correctly prints output.


### On Windows, when I start the application, nothing happens

The previous problem does not help to get a clue about the cause.

The most common is your device not being detected by `adb`, or is unauthorized.
Check everything is ok by calling:

    adb devices

Windows may need some [drivers] to detect your device.

[drivers]: https://developer.android.com/studio/run/oem-usb.html

If you still encounter problems, please see [issue 9].

[issue 9]: https://github.com/Genymobile/scrcpy/issues/9


### The application does not work over `adb connect`

If the device is connected using `adb connect`, then you'll get this error:

    error: more than one device/emulator
    ERROR: "adb reverse" returned with value 1

We plan to support it in future versions. See [issue 5].

[issue 5]: https://github.com/Genymobile/scrcpy/issues/5


### Mouse clicks do not work

On many LG devices, [mouse clicks do not work][issue 18]. We're [working][pr27]
on it.

[issue 18]: https://github.com/Genymobile/scrcpy/issues/18
[pr27]: https://github.com/Genymobile/scrcpy/pull/27


### I get a black screen for some applications like Silence

This is expected, they requested to protect the screen.

In [Silence], you can disable it in settings → Privacy → Screen security.

[silence]: https://f-droid.org/en/packages/org.smssecure.smssecure/

See [issue 36].

[issue 36]: https://github.com/Genymobile/scrcpy/issues/36


### Mouse clicks at wrong location

On MacOS, with HiDPI support and multiple screens, input location are wrongly
scaled. See [issue 15].

[issue 15]: https://github.com/Genymobile/scrcpy/issues/15

A workaround is to build with HiDPI support disabled:

```bash
meson x --buildtype release -Dhidpi_support=false
```

However, the video will be displayed at lower resolution.
