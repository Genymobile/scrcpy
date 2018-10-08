# scrcpy (v1.4)

This application provides display and control of Android devices connected on
USB (or [over TCP/IP][article-tcpip]). It does not require any _root_ access.
It works on _GNU/Linux_, _Windows_ and _MacOS_.

![screenshot](assets/screenshot-debian-600.jpg)


## Requirements

The Android part requires at least API 21 (Android 5.0).

Make sure you [enabled adb debugging][enable-adb] on your device(s).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling


## Get the app


### Linux

On Linux, you typically need to [build the app manually][BUILD]. Don't worry,
it's not that hard.

For Arch Linux, two [AUR] packages have been created by users:

 - [`scrcpy`](https://aur.archlinux.org/packages/scrcpy/)
 - [`scrcpy-prebuiltserver`](https://aur.archlinux.org/packages/scrcpy-prebuiltserver/)

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository

For Gentoo, an [Ebuild] is available: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy


### Windows

For Windows, for simplicity, prebuilt archives with all the dependencies
(including `adb`) are available:

 - [`scrcpy-win32-v1.4.zip`][direct-win32]  
   _(SHA-256: 1f72fa520980727e8943b7214b64c66b00b9b5267f7cffefb64fa37c3ca803cf)_
 - [`scrcpy-win64-v1.4.zip`][direct-win64]  
   _(SHA-256: 382f02bd8ed3db2cc7ab15aabdb83674744993b936d602b01e6959a150584a79)_

[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v1.4/scrcpy-win32-v1.4.zip
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.4/scrcpy-win64-v1.4.zip

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

For example, to decrease video bitrate to 2Mbps (default is 8Mbps):

```bash
scrcpy -b 2M
```

To limit the video dimensions (e.g. if the device is 2540×1440, but the host
screen is smaller, or cannot decode such a high definition):

```bash
scrcpy -m 1024
```

The device screen may be cropped to mirror only part of the screen:

```bash
scrcpy -c 1224:1440:0:0   # 1224x1440 at offset (0,0)
```

If several devices are listed in `adb devices`, you must specify the _serial_:

```bash
scrcpy -s 0123456789abcdef
```

To show physical touches while scrcpy is running:

```bash
scrcpy -t
```

The app may be started directly in fullscreen:

```
scrcpy -f
```

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
 | click on `VOLUME_UP`                   | `Ctrl`+`↑` _(up)_             |
 | click on `VOLUME_DOWN`                 | `Ctrl`+`↓` _(down)_           |
 | click on `POWER`                       | `Ctrl`+`p`                    |
 | turn screen on                         | _Right-click²_                |
 | paste computer clipboard to device     | `Ctrl`+`v`                    |
 | enable/disable FPS counter (on stdout) | `Ctrl`+`i`                    |
 | install APK from computer              | drag & drop APK file          |
 | push file to `/sdcard/`                | drag & drop non-APK file      |

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
