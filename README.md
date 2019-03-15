# scrcpy (v1.8)

This application provides display and control of Android devices connected on
USB (or [over TCP/IP][article-tcpip]). It does not require any _root_ access.
It works on _GNU/Linux_, _Windows_ and _MacOS_.

![screenshot](assets/screenshot-debian-600.jpg)


## Requirements

The Android part requires at least API 21 (Android 5.0).

Make sure you [enabled adb debugging][enable-adb] on your device(s).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

On some devices, you also need to enable [an additional option][control] to
control it using keyboard and mouse.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## Get the app


### Linux

On Linux, you typically need to [build the app manually][BUILD]. Don't worry,
it's not that hard.

For Arch Linux, an [AUR] package is available: [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

For Gentoo, an [Ebuild] is available: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy


### Windows

For Windows, for simplicity, prebuilt archives with all the dependencies
(including `adb`) are available:

 - [`scrcpy-win32-v1.8.zip`][direct-win32]  
   _(SHA-256: c0c29ed1c66deaa73bdadacd09e598aafb3a117929cf7a314cce1cc45e34de53)_
 - [`scrcpy-win64-v1.8.zip`][direct-win64]  
   _(SHA-256: 9cc980d07bd8f036ae4e91d0bc6fc3281d7fa8f9752d4913b643c0fb72a19fb7)_

[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v1.8/scrcpy-win32-v1.8.zip
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.8/scrcpy-win64-v1.8.zip

You can also [build the app manually][BUILD].


### Mac OS

The application is available in [Homebrew]. Just install it:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

You need `adb`, accessible from your `PATH`. If you don't have it yet:

```bash
brew cask install android-platform-tools
```

You can also [build the app manually][BUILD].


## Run

Plug an Android device, and execute:

```bash
scrcpy
```

It accepts command-line arguments, listed by:

```bash
scrcpy --help
```

## Features


### Reduce size

Sometimes, it is useful to mirror an Android device at a lower definition to
increase performances.

To limit both width and height to some value (e.g. 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # short version
```

The other dimension is computed to that the device aspect-ratio is preserved.
That way, a device in 1920×1080 will be mirrored at 1024×576.


### Change bit-rate

The default bit-rate is 8Mbps. To change the video bitrate (e.g. to 2Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # short version
```


### Crop

The device screen may be cropped to mirror only part of the screen.

This is useful for example to mirror only 1 eye of the Oculus Go:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 at offset (0,0)
scrcpy -c 1224:1440:0:0       # short version
```

If `--max-size` is also specified, resizing is applied after cropping.


### Wireless

_Scrcpy_ uses `adb` to communicate with the device, and `adb` can [connect] to a
device over TCP/IP:

1. Connect the device to the same Wi-Fi as your computer.
2. Get your device IP address (in Settings → About phone → Status).
3. Enable adb over TCP/IP on your device: `adb tcpip 5555`.
4. Unplug your device.
5. Connect to your device: `adb connect DEVICE_IP:5555` _(replace `DEVICE_IP`)_.
6. Run `scrcpy` as usual.

It may be useful to decrease the bit-rate and the definition:

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # short version
```

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


### Record screen

It is possible to record the screen while mirroring:

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

To disable mirroring while recording:

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# interrupt recording with Ctrl+C
# Ctrl+C does not terminate properly on Windows, so disconnect the device
```

"Skipped frames" are recorded, even if they are not displayed in real time (for
performance reasons). Frames are _timestamped_ on the device, so [packet delay
variation] does not impact the recorded file.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


### Multi-devices

If several devices are listed in `adb devices`, you must specify the _serial_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # short version
```

You can start several instances of _scrcpy_ for several devices.


### Fullscreen

The app may be started directly in fullscreen:

```bash
scrcpy --fullscreen
scrcpy -f  # short version
```

Fullscreen can then be toggled dynamically with `Ctrl`+`f`.


### Always on top

The window of app can always be above others by:

```bash
scrcpy --always-on-top
scrcpy -T  # short version
```


### Show touches

For presentations, it may be useful to show physical touches (on the physical
device).

Android provides this feature in _Developers options_.

_Scrcpy_ provides an option to enable this feature on start and disable on exit:

```bash
scrcpy --show-touches
scrcpy -t
```

Note that it only shows _physical_ touches (with the finger on the device).


### Install APK

To install an APK, drag & drop an APK file (ending with `.apk`) to the _scrcpy_
window.

There is no visual feedback, a log is printed to the console.


### Push file to device

To push a file to `/sdcard/` on the device, drag & drop a (non-APK) file to the
_scrcpy_ window.

There is no visual feedback, a log is printed to the console.


### Read-only

To disable controls (everything which can interact with the device: input keys,
mouse events, drag&drop files):

```bash
scrcpy --no-control
scrcpy -n
```


### Forward audio

Audio is not forwarded by _scrcpy_.

There is a limited solution using [AOA], implemented in the [`audio`] branch. If
you are interested, see [issue 14].


[AOA]: https://source.android.com/devices/accessories/aoa2
[`audio`]: https://github.com/Genymobile/scrcpy/commits/audio
[issue 14]: https://github.com/Genymobile/scrcpy/issues/14


## Shortcuts

 | Action                                 |   Shortcut                    |
 | -------------------------------------- |:----------------------------  |
 | switch fullscreen mode                 | `Ctrl`+`f`                    |
 | resize window to 1:1 (pixel-perfect)   | `Ctrl`+`g`                    |
 | resize window to remove black borders  | `Ctrl`+`x` \| _Double-click¹_ |
 | click on `HOME`                        | `Ctrl`+`h` \| _Middle-click_  |
 | click on `BACK`                        | `Ctrl`+`b` \| _Right-click²_  |
 | click on `APP_SWITCH`                  | `Ctrl`+`s`                    |
 | click on `MENU`                        | `Ctrl`+`m`                    |
 | click on `VOLUME_UP`                   | `Ctrl`+`↑` _(up)_   (`Cmd`+`↑` on MacOS) |
 | click on `VOLUME_DOWN`                 | `Ctrl`+`↓` _(down)_ (`Cmd`+`↓` on MacOS) |
 | click on `POWER`                       | `Ctrl`+`p`                    |
 | turn screen on                         | _Right-click²_                |
 | expand notification panel              | `Ctrl`+`n`                    |
 | collapse notification panel            | `Ctrl`+`Shift`+`n`            |
 | paste computer clipboard to device     | `Ctrl`+`v`                    |
 | enable/disable FPS counter (on stdout) | `Ctrl`+`i`                    |

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._


## Why _scrcpy_?

A colleague challenged me to find a name as unpronounceable as [gnirehtet].

[`strcpy`] copies a **str**ing; `scrcpy` copies a **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## How to build?

See [BUILD].

[BUILD]: BUILD.md


## Common issues

See the [FAQ](FAQ.md).


## Developers

Read the [developers page].

[developers page]: DEVELOP.md


## Licence

    Copyright (C) 2018 Genymobile

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

## Articles

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
