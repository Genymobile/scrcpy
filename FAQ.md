# Frequently Asked Questions

[Read in another language](#translations)

Here are the common reported problems and their status.

If you encounter any error, the first step is to upgrade to the latest version.


## `adb` and USB issues

`scrcpy` execute `adb` commands to initialize the connection with the device. If
`adb` fails, then scrcpy will not work.

This is typically not a bug in _scrcpy_, but a problem in your environment.


### `adb` not found

You need `adb` accessible from your `PATH`.

On Windows, the current directory is in your `PATH`, and `adb.exe` is included
in the release, so it should work out-of-the-box.


### Device not detected

>     ERROR: Could not find any ADB device

Check that you correctly enabled [adb debugging][enable-adb].

Your device must be detected by `adb`:

```
adb devices
```

If your device is not detected, you may need some [drivers] (on Windows). There is a separate [USB driver for Google devices][google-usb-driver].

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling
[drivers]: https://developer.android.com/studio/run/oem-usb.html
[google-usb-driver]: https://developer.android.com/studio/run/win-usb


### Device unauthorized

>     ERROR: Device is unauthorized:
>     ERROR:     -->   (usb)  0123456789abcdef          unauthorized
>     ERROR: A popup should open on the device to request authorization.

When connecting, a popup should open on the device. You must authorize USB
debugging.

If it does not open, check [stackoverflow][device-unauthorized].

[device-unauthorized]: https://stackoverflow.com/questions/23081263/adb-android-device-unauthorized


### Several devices connected

If several devices are connected, you will encounter this error:

>     ERROR: Multiple (2) ADB devices:
>     ERROR:     -->   (usb)  0123456789abcdef                device  Nexus_5
>     ERROR:     --> (tcpip)  192.168.1.5:5555                device  GM1913
>     ERROR: Select a device via -s (--serial), -d (--select-usb) or -e (--select-tcpip)

In that case, you can either provide the identifier of the device you want to
mirror:

```bash
scrcpy -s 0123456789abcdef
```

Or request the single USB (or TCP/IP) device:

```bash
scrcpy -d  # USB device
scrcpy -e  # TCP/IP device
```

Note that if your device is connected over TCP/IP, you might get this message:

>     adb: error: more than one device/emulator
>     ERROR: "adb reverse" returned with value 1
>     WARN: 'adb reverse' failed, fallback to 'adb forward'

This is expected (due to a bug on old Android versions, see [#5]), but in that
case, scrcpy fallbacks to a different method, which should work.

[#5]: https://github.com/Genymobile/scrcpy/issues/5


### Conflicts between adb versions

>     adb server version (41) doesn't match this client (39); killing...

This error occurs when you use several `adb` versions simultaneously. You must
find the program using a different `adb` version, and use the same `adb` version
everywhere.

You could overwrite the `adb` binary in the other program, or ask _scrcpy_ to
use a specific `adb` binary, by setting the `ADB` environment variable:

```bash
# in bash
export ADB=/path/to/your/adb
scrcpy
```

```cmd
:: in cmd
set ADB=C:\path\to\your\adb.exe
scrcpy
```

```powershell
# in PowerShell
$env:ADB = 'C:\path\to\your\adb.exe'
scrcpy
```


### Device disconnected

If _scrcpy_ stops itself with the warning "Device disconnected", then the
`adb` connection has been closed.

Try with another USB cable or plug it into another USB port. See [#281] and
[#283].

[#281]: https://github.com/Genymobile/scrcpy/issues/281
[#283]: https://github.com/Genymobile/scrcpy/issues/283


## OTG issues on Windows

On Windows, if `scrcpy --otg` (or `--keyboard=aoa`/`--mouse=aoa`) results in:

>     ERROR: Could not find any USB device

(or if only unrelated USB devices are detected), there might be drivers issues.

Please read [#3654], in particular [this comment][#3654-comment1] and [the next
one][#3654-comment2].

[#3654]: https://github.com/Genymobile/scrcpy/issues/3654
[#3654-comment1]: https://github.com/Genymobile/scrcpy/issues/3654#issuecomment-1369278232
[#3654-comment2]: https://github.com/Genymobile/scrcpy/issues/3654#issuecomment-1369295011


## Control issues

### Mouse and keyboard do not work

On some devices, you may need to enable an option to allow [simulating input].
In developer options, enable:

> **USB debugging (Security settings)**  
> _Allow granting permissions and simulating input via USB debugging_

Rebooting the device is necessary once this option is set.

[simulating input]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


### Special characters do not work

The default text injection method is limited to ASCII characters. A trick allows
to also inject some [accented characters][accented-characters],
but that's all. See [#37].

To avoid the problem, [change the keyboard mode to simulate a physical
keyboard][hid].

[accented-characters]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-accented-characters
[#37]: https://github.com/Genymobile/scrcpy/issues/37
[hid]: doc/keyboard.md#physical-keyboard-simulation


## Client issues

### Issue with Wayland

By default, SDL uses x11 on Linux. The [video driver] can be changed via the
`SDL_VIDEODRIVER` environment variable:

[video driver]: https://wiki.libsdl.org/FAQUsingSDL#how_do_i_choose_a_specific_video_driver

```bash
export SDL_VIDEODRIVER=wayland
scrcpy
```

On some distributions (at least Fedora), the package `libdecor` must be
installed manually.

See issues [#2554] and [#2559].

[#2554]: https://github.com/Genymobile/scrcpy/issues/2554
[#2559]: https://github.com/Genymobile/scrcpy/issues/2559


### KWin compositor crashes

On Plasma Desktop, compositor is disabled while _scrcpy_ is running.

As a workaround, [disable "Block compositing"][kwin].

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613


## Crashes

### Exception

If you get any exception related to `MediaCodec`:

```
ERROR: Exception on thread Thread[main,5,main]
java.lang.IllegalStateException
        at android.media.MediaCodec.native_dequeueOutputBuffer(Native Method)
```

then try with another [encoder](doc/video.md#encoder).


## Translations

Translations of this FAQ in other languages are available in the [wiki].

[wiki]: https://github.com/Genymobile/scrcpy/wiki

Only this FAQ file is guaranteed to be up-to-date.
