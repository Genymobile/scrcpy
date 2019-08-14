# Frequently Asked Questions

Here are the common reported problems and their status.


### On Windows, my device is not detected

The most common is your device not being detected by `adb`, or is unauthorized.
Check everything is ok by calling:

    adb devices

Windows may need some [drivers] to detect your device.

[drivers]: https://developer.android.com/studio/run/oem-usb.html


### I can only mirror, I cannot interact with the device

On some devices, you may need to enable an option to allow [simulating input].
In developer options, enable:

> **USB debugging (Security settings)**  
> _Allow granting permissions and simulating input via USB debugging_

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


### The quality is low on HiDPI display

On Windows, you may need to configure the [scaling behavior].

> `scrcpy.exe` > Properties > Compatibility > Change high DPI settings >
> Override high DPI scaling behavior > Scaling performed by: _Application_.

[scaling behavior]: https://github.com/Genymobile/scrcpy/issues/40#issuecomment-424466723


### KWin compositor crashes

On Plasma Desktop, compositor is disabled while _scrcpy_ is running.

As a workaround, [disable "Block compositing"][kwin].

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613


### I get an error "Could not open video stream"

There may be many reasons. One common cause is that the hardware encoder of your
device is not able to encode at the given definition:

```
ERROR: Exception on thread Thread[main,5,main]
android.media.MediaCodec$CodecException: Error 0xfffffc0e
...
Exit due to uncaughtException in main thread:
ERROR: Could not open video stream
INFO: Initial texture: 1080x2336
```

Just try with a lower definition:

```
scrcpy -m 1920
scrcpy -m 1024
scrcpy -m 800
```
