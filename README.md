# scrcpy (v1.14)

This application provides display and control of Android devices connected on
USB (or [over TCP/IP][article-tcpip]). It does not require any _root_ access.
It works on _GNU/Linux_, _Windows_ and _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

It focuses on:

 - **lightness** (native, displays only the device screen)
 - **performance** (30~60fps)
 - **quality** (1920×1080 or above)
 - **low latency** ([35~70ms][lowlatency])
 - **low startup time** (~1 second to display the first image)
 - **non-intrusiveness** (nothing is left installed on the device)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## Requirements

The Android device requires at least API 21 (Android 5.0).

Make sure you [enabled adb debugging][enable-adb] on your device(s).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

On some devices, you also need to enable [an additional option][control] to
control it using keyboard and mouse.

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## Get the app


### Linux

On Debian (_testing_ and _sid_ for now) and Ubuntu (20.04):

```
apt install scrcpy
```

A [Snap] package is available: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

For Fedora, a [COPR] package is available: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

For Arch Linux, an [AUR] package is available: [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

For Gentoo, an [Ebuild] is available: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

You could also [build the app manually][BUILD] (don't worry, it's not that
hard).



### Windows

For Windows, for simplicity, a prebuilt archive with all the dependencies
(including `adb`) is available:

 - [`scrcpy-win64-v1.14.zip`][direct-win64]  
   _(SHA-256: 2be9139e46e29cf2f5f695848bb2b75a543b8f38be1133257dc5068252abc25f)_

[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.14/scrcpy-win64-v1.14.zip

It is also available in [Chocolatey]:

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # if you don't have it yet
```

And in [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # if you don't have it yet
```

[Scoop]: https://scoop.sh

You can also [build the app manually][BUILD].


### macOS

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

### Capture configuration

#### Reduce size

Sometimes, it is useful to mirror an Android device at a lower definition to
increase performance.

To limit both the width and height to some value (e.g. 1024):

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # short version
```

The other dimension is computed to that the device aspect ratio is preserved.
That way, a device in 1920×1080 will be mirrored at 1024×576.


#### Change bit-rate

The default bit-rate is 8 Mbps. To change the video bitrate (e.g. to 2 Mbps):

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # short version
```

#### Limit frame rate

The capture frame rate can be limited:

```bash
scrcpy --max-fps 15
```

This is officially supported since Android 10, but may work on earlier versions.

#### Crop

The device screen may be cropped to mirror only part of the screen.

This is useful for example to mirror only one eye of the Oculus Go:

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 at offset (0,0)
```

If `--max-size` is also specified, resizing is applied after cropping.


#### Lock video orientation


To lock the orientation of the mirroring:

```bash
scrcpy --lock-video-orientation 0   # natural orientation
scrcpy --lock-video-orientation 1   # 90° counterclockwise
scrcpy --lock-video-orientation 2   # 180°
scrcpy --lock-video-orientation 3   # 90° clockwise
```

This affects recording orientation.


### Recording

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
```

"Skipped frames" are recorded, even if they are not displayed in real time (for
performance reasons). Frames are _timestamped_ on the device, so [packet delay
variation] does not impact the recorded file.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


### Connection

#### Wireless

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


#### Multi-devices

If several devices are listed in `adb devices`, you must specify the _serial_:

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # short version
```

If the device is connected over TCP/IP:

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # short version
```

You can start several instances of _scrcpy_ for several devices.

#### Autostart on device connection

You could use [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### SSH tunnel

To connect to a remote device, it is possible to connect a local `adb` client to
a remote `adb` server (provided they use the same version of the _adb_
protocol):

```bash
adb kill-server    # kill the local adb server on 5037
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal:

```bash
scrcpy
```

To avoid enabling remote port forwarding, you could force a forward connection
instead (notice the `-L` instead of `-R`):

```bash
adb kill-server    # kill the local adb server on 5037
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal:

```bash
scrcpy --force-adb-forward
```


Like for wireless connections, it may be useful to reduce quality:

```
scrcpy -b2M -m800 --max-fps 15
```

### Window configuration

#### Title

By default, the window title is the device model. It can be changed:

```bash
scrcpy --window-title 'My device'
```

#### Position and size

The initial window position and size may be specified:

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### Borderless

To disable window decorations:

```bash
scrcpy --window-borderless
```

#### Always on top

To keep the scrcpy window always on top:

```bash
scrcpy --always-on-top
```

#### Fullscreen

The app may be started directly in fullscreen:

```bash
scrcpy --fullscreen
scrcpy -f  # short version
```

Fullscreen can then be toggled dynamically with `Ctrl`+`f`.

#### Rotation

The window may be rotated:

```bash
scrcpy --rotation 1
```

Possibles values are:
 - `0`: no rotation
 - `1`: 90 degrees counterclockwise
 - `2`: 180 degrees
 - `3`: 90 degrees clockwise

The rotation can also be changed dynamically with `Ctrl`+`←` _(left)_ and
`Ctrl`+`→` _(right)_.

Note that _scrcpy_ manages 3 different rotations:
 - `Ctrl`+`r` requests the device to switch between portrait and landscape (the
   current running app may refuse, if it does support the requested
   orientation).
 - `--lock-video-orientation` changes the mirroring orientation (the orientation
   of the video sent from the device to the computer). This affects the
   recording.
 - `--rotation` (or `Ctrl`+`←`/`Ctrl`+`→`) rotates only the window content. This
   affects only the display, not the recording.


### Other mirroring options

#### Read-only

To disable controls (everything which can interact with the device: input keys,
mouse events, drag&drop files):

```bash
scrcpy --no-control
scrcpy -n
```

#### Display

If several displays are available, it is possible to select the display to
mirror:

```bash
scrcpy --display 1
```

The list of display ids can be retrieved by:

```
adb shell dumpsys display   # search "mDisplayId=" in the output
```

The secondary display may only be controlled if the device runs at least Android
10 (otherwise it is mirrored in read-only).


#### Stay awake

To prevent the device to sleep after some delay when the device is plugged in:

```bash
scrcpy --stay-awake
scrcpy -w
```

The initial state is restored when scrcpy is closed.


#### Turn screen off

It is possible to turn the device screen off while mirroring on start with a
command-line option:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Or by pressing `Ctrl`+`o` at any time.

To turn it back on, press `Ctrl`+`Shift`+`o` (or `POWER`, `Ctrl`+`p`).

It can also be useful to prevent the device from sleeping:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```


#### Render expired frames

By default, to minimize latency, _scrcpy_ always renders the last decoded frame
available, and drops any previous one.

To force the rendering of all frames (at a cost of a possible increased
latency), use:

```bash
scrcpy --render-expired-frames
```

#### Show touches

For presentations, it may be useful to show physical touches (on the physical
device).

Android provides this feature in _Developers options_.

_Scrcpy_ provides an option to enable this feature on start and restore the
initial value on exit:

```bash
scrcpy --show-touches
scrcpy -t
```

Note that it only shows _physical_ touches (with the finger on the device).


### Input control

#### Rotate device screen

Press `Ctrl`+`r` to switch between portrait and landscape modes.

Note that it rotates only if the application in foreground supports the
requested orientation.

#### Copy-paste

It is possible to synchronize clipboards between the computer and the device, in
both directions:

 - `Ctrl`+`c` copies the device clipboard to the computer clipboard;
 - `Ctrl`+`Shift`+`v` copies the computer clipboard to the device clipboard (and
   pastes if the device runs Android >= 7);
 - `Ctrl`+`v` _pastes_ the computer clipboard as a sequence of text events (but
   breaks non-ASCII characters).

Moreover, any time the Android clipboard changes, it is automatically
synchronized to the computer clipboard.

#### Text injection preference

There are two kinds of [events][textevents] generated when typing text:
 - _key events_, signaling that a key is pressed or released;
 - _text events_, signaling that a text has been entered.

By default, letters are injected using key events, so that the keyboard behaves
as expected in games (typically for WASD keys).

But this may [cause issues][prefertext]. If you encounter such a problem, you
can avoid it by:

```bash
scrcpy --prefer-text
```

(but this will break keyboard behavior in games)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


### File drop

#### Install APK

To install an APK, drag & drop an APK file (ending with `.apk`) to the _scrcpy_
window.

There is no visual feedback, a log is printed to the console.


#### Push file to device

To push a file to `/sdcard/` on the device, drag & drop a (non-APK) file to the
_scrcpy_ window.

There is no visual feedback, a log is printed to the console.

The target directory can be changed on start:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### Audio forwarding

Audio is not forwarded by _scrcpy_. Use [sndcpy].

Also see [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Shortcuts

 | Action                                      |   Shortcut                    |   Shortcut (macOS)
 | ------------------------------------------- |:----------------------------- |:-----------------------------
 | Switch fullscreen mode                      | `Ctrl`+`f`                    | `Cmd`+`f`
 | Rotate display left                         | `Ctrl`+`←` _(left)_           | `Cmd`+`←` _(left)_
 | Rotate display right                        | `Ctrl`+`→` _(right)_          | `Cmd`+`→` _(right)_
 | Resize window to 1:1 (pixel-perfect)        | `Ctrl`+`g`                    | `Cmd`+`g`
 | Resize window to remove black borders       | `Ctrl`+`x` \| _Double-click¹_ | `Cmd`+`x`  \| _Double-click¹_
 | Click on `HOME`                             | `Ctrl`+`h` \| _Middle-click_  | `Ctrl`+`h` \| _Middle-click_
 | Click on `BACK`                             | `Ctrl`+`b` \| _Right-click²_  | `Cmd`+`b`  \| _Right-click²_
 | Click on `APP_SWITCH`                       | `Ctrl`+`s`                    | `Cmd`+`s`
 | Click on `MENU`                             | `Ctrl`+`m`                    | `Ctrl`+`m`
 | Click on `VOLUME_UP`                        | `Ctrl`+`↑` _(up)_             | `Cmd`+`↑` _(up)_
 | Click on `VOLUME_DOWN`                      | `Ctrl`+`↓` _(down)_           | `Cmd`+`↓` _(down)_
 | Click on `POWER`                            | `Ctrl`+`p`                    | `Cmd`+`p`
 | Power on                                    | _Right-click²_                | _Right-click²_
 | Turn device screen off (keep mirroring)     | `Ctrl`+`o`                    | `Cmd`+`o`
 | Turn device screen on                       | `Ctrl`+`Shift`+`o`            | `Cmd`+`Shift`+`o`
 | Rotate device screen                        | `Ctrl`+`r`                    | `Cmd`+`r`
 | Expand notification panel                   | `Ctrl`+`n`                    | `Cmd`+`n`
 | Collapse notification panel                 | `Ctrl`+`Shift`+`n`            | `Cmd`+`Shift`+`n`
 | Copy device clipboard to computer           | `Ctrl`+`c`                    | `Cmd`+`c`
 | Paste computer clipboard to device          | `Ctrl`+`v`                    | `Cmd`+`v`
 | Copy computer clipboard to device and paste | `Ctrl`+`Shift`+`v`            | `Cmd`+`Shift`+`v`
 | Enable/disable FPS counter (on stdout)      | `Ctrl`+`i`                    | `Cmd`+`i`

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._


## Custom paths

To use a specific _adb_ binary, configure its path in the environment variable
`ADB`:

    ADB=/path/to/adb scrcpy

To override the path of the `scrcpy-server` file, configure its path in
`SCRCPY_SERVER_PATH`.

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


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
    Copyright (C) 2018-2020 Romain Vimont

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
