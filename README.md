# scrcpy (v1.17)

[Read in another language](#translations)

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

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Summary

 - Linux: `apt install scrcpy`
 - Windows: [download][direct-win64]
 - macOS: `brew install scrcpy`

Build from sources: [BUILD] ([simplified process][BUILD_simple])

[BUILD]: BUILD.md
[BUILD_simple]: BUILD.md#simple


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

You could also [build the app manually][BUILD] ([simplified
process][BUILD_simple]).


### Windows

For Windows, for simplicity, a prebuilt archive with all the dependencies
(including `adb`) is available:

 - [`scrcpy-win64-v1.17.zip`][direct-win64]  
   _(SHA-256: 8b9e57993c707367ed10ebfe0e1ef563c7a29d9af4a355cd8b6a52a317c73eea)_

[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.17/scrcpy-win64-v1.17.zip

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
brew install android-platform-tools
```

It's also available in [MacPorts], which sets up adb for you:

```bash
sudo port install scrcpy
```

[MacPorts]: https://www.macports.org/


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

The [window may also be rotated](#rotation) independently.


#### Encoder

Some devices have more than one encoder, and some of them may cause issues or
crash. It is possible to select a different encoder:

```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

To list the available encoders, you could pass an invalid encoder name, the
error will give the available encoders:

```bash
scrcpy --encoder _
```

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
2. Get your device IP address, in Settings → About phone → Status, or by
   executing this command:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

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

Fullscreen can then be toggled dynamically with <kbd>MOD</kbd>+<kbd>f</kbd>.

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

The rotation can also be changed dynamically with <kbd>MOD</kbd>+<kbd>←</kbd>
_(left)_ and <kbd>MOD</kbd>+<kbd>→</kbd> _(right)_.

Note that _scrcpy_ manages 3 different rotations:
 - <kbd>MOD</kbd>+<kbd>r</kbd> requests the device to switch between portrait
   and landscape (the current running app may refuse, if it does not support the
   requested orientation).
 - [`--lock-video-orientation`](#lock-video-orientation) changes the mirroring
   orientation (the orientation of the video sent from the device to the
   computer). This affects the recording.
 - `--rotation` (or <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>)
   rotates only the window content. This affects only the display, not the
   recording.


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

```bash
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

Or by pressing <kbd>MOD</kbd>+<kbd>o</kbd> at any time.

To turn it back on, press <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

On Android, the `POWER` button always turns the screen on. For convenience, if
`POWER` is sent via scrcpy (via right-click or <kbd>MOD</kbd>+<kbd>p</kbd>), it
will force to turn the screen off after a small delay (on a best effort basis).
The physical `POWER` button will still cause the screen to be turned on.

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


#### Disable screensaver

By default, scrcpy does not prevent the screensaver to run on the computer.

To disable it:

```bash
scrcpy --disable-screensaver
```


### Input control

#### Rotate device screen

Press <kbd>MOD</kbd>+<kbd>r</kbd> to switch between portrait and landscape
modes.

Note that it rotates only if the application in foreground supports the
requested orientation.

#### Copy-paste

Any time the Android clipboard changes, it is automatically synchronized to the
computer clipboard.

Any <kbd>Ctrl</kbd> shortcut is forwarded to the device. In particular:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> typically copies
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> typically cuts
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> typically pastes (after computer-to-device
   clipboard synchronization)

This typically works as you expect.

The actual behavior depends on the active application though. For example,
_Termux_ sends SIGINT on <kbd>Ctrl</kbd>+<kbd>c</kbd> instead, and _K-9 Mail_
composes a new message.

To copy, cut and paste in such cases (but only supported on Android >= 7):
 - <kbd>MOD</kbd>+<kbd>c</kbd> injects `COPY`
 - <kbd>MOD</kbd>+<kbd>x</kbd> injects `CUT`
 - <kbd>MOD</kbd>+<kbd>v</kbd> injects `PASTE` (after computer-to-device
   clipboard synchronization)

In addition, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> allows to inject the
computer clipboard text as a sequence of key events. This is useful when the
component does not accept text pasting (for example in _Termux_), but it can
break non-ASCII content.

**WARNING:** Pasting the computer clipboard to the device (either via
<kbd>Ctrl</kbd>+<kbd>v</kbd> or <kbd>MOD</kbd>+<kbd>v</kbd>) copies the content
into the device clipboard. As a consequence, any Android application could read
its content. You should avoid to paste sensitive content (like passwords) that
way.

Some devices do not behave as expected when setting the device clipboard
programmatically. An option `--legacy-paste` is provided to change the behavior
of <kbd>Ctrl</kbd>+<kbd>v</kbd> and <kbd>MOD</kbd>+<kbd>v</kbd> so that they
also inject the computer clipboard text as a sequence of key events (the same
way as <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>).

#### Pinch-to-zoom

To simulate "pinch-to-zoom": <kbd>Ctrl</kbd>+_click-and-move_.

More precisely, hold <kbd>Ctrl</kbd> while pressing the left-click button. Until
the left-click button is released, all mouse movements scale and rotate the
content (if supported by the app) relative to the center of the screen.

Concretely, scrcpy generates additional touch events from a "virtual finger" at
a location inverted through the center of the screen.


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


#### Key repeat

By default, holding a key down generates repeated key events. This can cause
performance problems in some games, where these events are useless anyway.

To avoid forwarding repeated key events:

```bash
scrcpy --no-key-repeat
```


#### Right-click and middle-click

By default, right-click triggers BACK (or POWER on) and middle-click triggers
HOME. To disable these shortcuts and forward the clicks to the device instead:

```bash
scrcpy --forward-all-clicks
```


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
scrcpy --push-target=/sdcard/Download/
```


### Audio forwarding

Audio is not forwarded by _scrcpy_. Use [sndcpy].

Also see [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Shortcuts

In the following list, <kbd>MOD</kbd> is the shortcut modifier. By default, it's
(left) <kbd>Alt</kbd> or (left) <kbd>Super</kbd>.

It can be changed using `--shortcut-mod`. Possible keys are `lctrl`, `rctrl`,
`lalt`, `ralt`, `lsuper` and `rsuper`. For example:

```bash
# use RCtrl for shortcuts
scrcpy --shortcut-mod=rctrl

# use either LCtrl+LAlt or LSuper for shortcuts
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> is typically the <kbd>Windows</kbd> or <kbd>Cmd</kbd> key._

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | Action                                      |   Shortcut
 | ------------------------------------------- |:-----------------------------
 | Switch fullscreen mode                      | <kbd>MOD</kbd>+<kbd>f</kbd>
 | Rotate display left                         | <kbd>MOD</kbd>+<kbd>←</kbd> _(left)_
 | Rotate display right                        | <kbd>MOD</kbd>+<kbd>→</kbd> _(right)_
 | Resize window to 1:1 (pixel-perfect)        | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Resize window to remove black borders       | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Double-click¹_
 | Click on `HOME`                             | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Middle-click_
 | Click on `BACK`                             | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Right-click²_
 | Click on `APP_SWITCH`                       | <kbd>MOD</kbd>+<kbd>s</kbd>
 | Click on `MENU` (unlock screen)             | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Click on `VOLUME_UP`                        | <kbd>MOD</kbd>+<kbd>↑</kbd> _(up)_
 | Click on `VOLUME_DOWN`                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(down)_
 | Click on `POWER`                            | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Power on                                    | _Right-click²_
 | Turn device screen off (keep mirroring)     | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Turn device screen on                       | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Rotate device screen                        | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Expand notification panel                   | <kbd>MOD</kbd>+<kbd>n</kbd>
 | Collapse notification panel                 | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copy to clipboard³                          | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Cut to clipboard³                           | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Synchronize clipboards and paste³           | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Inject computer clipboard text              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Enable/disable FPS counter (on stdout)      | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pinch-to-zoom                               | <kbd>Ctrl</kbd>+_click-and-move_

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._  
_³Only on Android >= 7._

All <kbd>Ctrl</kbd>+_key_ shortcuts are forwarded to the device, so they are
handled by the active application.


## Custom paths

To use a specific _adb_ binary, configure its path in the environment variable
`ADB`:

```bash
ADB=/path/to/adb scrcpy
```

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


## Common issues

See the [FAQ](FAQ.md).


## Developers

Read the [developers page].

[developers page]: DEVELOP.md


## Licence

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2021 Romain Vimont

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

## Translations

This README is available in other languages:

- [Indonesian (Indonesia, `id`) - v1.16](README.id.md)
- [Italiano (Italiano, `it`) - v1.17](README.it.md)
- [日本語 (Japanese, `jp`) - v1.17](README.jp.md)
- [한국어 (Korean, `ko`) - v1.11](README.ko.md)
- [português brasileiro (Brazilian Portuguese, `pt-BR`) - v1.17](README.pt-br.md)
- [Español (Spanish, `sp`) - v1.17](README.sp.md)
- [简体中文 (Simplified Chinese, `zh-Hans`) - v1.17](README.zh-Hans.md)
- [繁體中文 (Traditional Chinese, `zh-Hant`) - v1.15](README.zh-Hant.md)

Only this README file is guaranteed to be up-to-date.
