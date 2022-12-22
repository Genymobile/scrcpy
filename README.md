# scrcpy (v1.25)

<img src="app/data/icon.svg" width="128" height="128" alt="scrcpy" align="right" />

_pronounced "**scr**een **c**o**py**"_

[Read in another language](#translations)

This application provides display and control of Android devices connected via
USB or [over TCP/IP](#tcpip-wireless). It does not require any _root_ access.
It works on _GNU/Linux_, _Windows_ and _macOS_.

![screenshot](assets/screenshot-debian-600.jpg)

It focuses on:

 - **lightness**: native, displays only the device screen
 - **performance**: 30~120fps, depending on the device
 - **quality**: 1920×1080 or above
 - **low latency**: [35~70ms][lowlatency]
 - **low startup time**: ~1 second to display the first image
 - **non-intrusiveness**: nothing is left installed on the Android device
 - **user benefits**: no account, no ads, no internet required
 - **freedom**: free and open source software

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646

Its features include:
 - [recording](#recording)
 - mirroring with [Android device screen off](#turn-screen-off)
 - [copy-paste](#copy-paste) in both directions
 - [configurable quality](#capture-configuration)
 - Android device [as a webcam (V4L2)](#v4l2loopback) (Linux-only)
 - [physical keyboard simulation (HID)](#physical-keyboard-simulation-hid)
 - [physical mouse simulation (HID)](#physical-mouse-simulation-hid)
 - [OTG mode](#otg)
 - and more…

## Requirements

The Android device requires at least API 21 (Android 5.0).

Make sure you [enable adb debugging][enable-adb] on your device(s).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

On some devices, you also need to enable [an additional option][control] to
control it using a keyboard and mouse.

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

On Debian and Ubuntu:

```
apt install scrcpy
```

On Arch Linux:

```
pacman -S scrcpy
```

A [Snap] package is available: [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

For Fedora, a [COPR] package is available: [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/


For Gentoo, an [Ebuild] is available: [`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

You can also [build the app manually][BUILD] ([simplified
process][BUILD_simple]).


### Windows

For Windows, a prebuilt archive with all the dependencies (including `adb`) is
available:

 - [`scrcpy-win64-v1.25.zip`][direct-win64]  
   <sub>SHA-256: `db65125e9c65acd00359efb7cea9c05f63cc7ccd5833000cd243cc92f5053028`</sub>

[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.25/scrcpy-win64-v1.25.zip

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

It's also available in [MacPorts], which sets up `adb` for you:

```bash
sudo port install scrcpy
```

[MacPorts]: https://www.macports.org/


You can also [build the app manually][BUILD].


## Run

Plug an Android device into your computer, and execute:

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

Sometimes, it is useful to mirror an Android device at a lower resolution to
increase performance.

To limit both the width and height to some value (e.g. 1024):

```bash
scrcpy --max-size=1024
scrcpy -m 1024  # short version
```

The other dimension is computed so that the Android device aspect ratio is
preserved. That way, a device in 1920×1080 will be mirrored at 1024×576.


#### Change bit-rate

The default bit-rate is 8 Mbps. To change the video bitrate (e.g. to 2 Mbps):

```bash
scrcpy --bit-rate=2M
scrcpy -b 2M  # short version
```

#### Limit frame rate

The capture frame rate can be limited:

```bash
scrcpy --max-fps=15
```

This is officially supported since Android 10, but may work on earlier versions.

The actual capture framerate may be printed to the console:

```
scrcpy --print-fps
```

It may also be enabled or disabled at any time with <kbd>MOD</kbd>+<kbd>i</kbd>.


#### Crop

The device screen may be cropped to mirror only part of the screen.

This is useful, for example, to mirror only one eye of the Oculus Go:

```bash
scrcpy --crop=1224:1440:0:0   # 1224x1440 at offset (0,0)
```

If `--max-size` is also specified, resizing is applied after cropping.


#### Lock video orientation

To lock the orientation of the mirroring:

```bash
scrcpy --lock-video-orientation     # initial (current) orientation
scrcpy --lock-video-orientation=0   # natural orientation
scrcpy --lock-video-orientation=1   # 90° counterclockwise
scrcpy --lock-video-orientation=2   # 180°
scrcpy --lock-video-orientation=3   # 90° clockwise
```

This affects recording orientation.

The [window may also be rotated](#rotation) independently.


#### Encoder

Some devices have more than one encoder, and some of them may cause issues or
crash. It is possible to select a different encoder:

```bash
scrcpy --encoder=OMX.qcom.video.encoder.avc
```

To list the available encoders, you can pass an invalid encoder name; the
error will give the available encoders:

```bash
scrcpy --encoder=_
```

### Capture

#### Recording

It is possible to record the screen while mirroring:

```bash
scrcpy --record=file.mp4
scrcpy -r file.mkv
```

To disable mirroring while recording:

```bash
scrcpy --no-display --record=file.mp4
scrcpy -Nr file.mkv
# interrupt recording with Ctrl+C
```

"Skipped frames" are recorded, even if they are not displayed in real time (for
performance reasons). Frames are _timestamped_ on the device, so [packet delay
variation] does not impact the recorded file.

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


#### v4l2loopback

On Linux, it is possible to send the video stream to a v4l2 loopback device, so
that the Android device can be opened like a webcam by any v4l2-capable tool.

The module `v4l2loopback` must be installed:

```bash
sudo apt install v4l2loopback-dkms
```

To create a v4l2 device:

```bash
sudo modprobe v4l2loopback
```

This will create a new video device in `/dev/videoN`, where `N` is an integer
(more [options](https://github.com/umlaeute/v4l2loopback#options) are available
to create several devices or devices with specific IDs).

To list the enabled devices:

```bash
# requires v4l-utils package
v4l2-ctl --list-devices

# simple but might be sufficient
ls /dev/video*
```

To start `scrcpy` using a v4l2 sink:

```bash
scrcpy --v4l2-sink=/dev/videoN
scrcpy --v4l2-sink=/dev/videoN --no-display  # disable mirroring window
scrcpy --v4l2-sink=/dev/videoN -N            # short version
```

(replace `N` with the device ID, check with `ls /dev/video*`)

Once enabled, you can open your video stream with a v4l2-capable tool:

```bash
ffplay -i /dev/videoN
vlc v4l2:///dev/videoN   # VLC might add some buffering delay
```

For example, you could capture the video within [OBS].

[OBS]: https://obsproject.com/


#### Buffering

It is possible to add buffering. This increases latency, but reduces jitter (see
[#2464]).

[#2464]: https://github.com/Genymobile/scrcpy/issues/2464

The option is available for display buffering:

```bash
scrcpy --display-buffer=50  # add 50 ms buffering for display
```

and V4L2 sink:

```bash
scrcpy --v4l2-buffer=500    # add 500 ms buffering for v4l2 sink
```


### Connection

#### TCP/IP (wireless)

_Scrcpy_ uses `adb` to communicate with the device, and `adb` can [connect] to a
device over TCP/IP. The device must be connected on the same network as the
computer.

##### Automatic

An option `--tcpip` allows to configure the connection automatically. There are
two variants.

If the device (accessible at 192.168.1.1 in this example) already listens on a
port (typically 5555) for incoming _adb_ connections, then run:

```bash
scrcpy --tcpip=192.168.1.1       # default port is 5555
scrcpy --tcpip=192.168.1.1:5555
```

If _adb_ TCP/IP mode is disabled on the device (or if you don't know the IP
address), connect the device over USB, then run:

```bash
scrcpy --tcpip    # without arguments
```

It will automatically find the device IP address and adb port, enable TCP/IP
mode if necessary, then connect to the device before starting.

##### Manual

Alternatively, it is possible to enable the TCP/IP connection manually using
`adb`:

1. Plug the device into a USB port on your computer.
2. Connect the device to the same Wi-Fi network as your computer.
3. Get your device IP address, in Settings → About phone → Status, or by
   executing this command:

    ```bash
    adb shell ip route | awk '{print $9}'
    ```

4. Enable `adb` over TCP/IP on your device: `adb tcpip 5555`.
5. Unplug your device.
6. Connect to your device: `adb connect DEVICE_IP:5555` _(replace `DEVICE_IP`
with the device IP address you found)_.
7. Run `scrcpy` as usual.

Since Android 11, a [Wireless debugging option][adb-wireless] allows to bypass
having to physically connect your device directly to your computer.

[adb-wireless]: https://developer.android.com/studio/command-line/adb#connect-to-a-device-over-wi-fi-android-11+

If the connection randomly drops, run your `scrcpy` command to reconnect. If it
says there are no devices/emulators found, try running `adb connect
DEVICE_IP:5555` again, and then `scrcpy` as usual. If it still says there are
none found, try running `adb disconnect`, and then run those two commands again.

It may be useful to decrease the bit-rate and the resolution:

```bash
scrcpy --bit-rate=2M --max-size=800
scrcpy -b2M -m800  # short version
```

[connect]: https://developer.android.com/studio/command-line/adb.html#wireless


#### Multi-devices

If several devices are listed in `adb devices`, you can specify the _serial_:

```bash
scrcpy --serial=0123456789abcdef
scrcpy -s 0123456789abcdef  # short version
```

The serial may also be provided via the environment variable `ANDROID_SERIAL`
(also used by `adb`).

If the device is connected over TCP/IP:

```bash
scrcpy --serial=192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # short version
```

If only one device is connected via either USB or TCP/IP, it is possible to
select it automatically:

```bash
# Select the only device connected via USB
scrcpy -d             # like adb -d
scrcpy --select-usb   # long version

# Select the only device connected via TCP/IP
scrcpy -e             # like adb -e
scrcpy --select-tcpip # long version
```

You can start several instances of _scrcpy_ for several devices.

#### Autostart on device connection

You could use [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### Tunnels

To connect to a remote device, it is possible to connect a local `adb` client to
a remote `adb` server (provided they use the same version of the _adb_
protocol).

##### Remote ADB server

To connect to a remote _adb server_, make the server listen on all interfaces:

```bash
adb kill-server
adb -a nodaemon server start
# keep this open
```

**Warning: all communications between clients and the _adb server_ are
unencrypted.**

Suppose that this server is accessible at 192.168.1.2. Then, from another
terminal, run `scrcpy`:

```bash
# in bash
export ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:192.168.1.2:5037
scrcpy --tunnel-host=192.168.1.2
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:192.168.1.2:5037'
scrcpy --tunnel-host=192.168.1.2
```

By default, `scrcpy` uses the local port used for `adb forward` tunnel
establishment (typically `27183`, see `--port`). It is also possible to force a
different tunnel port (it may be useful in more complex situations, when more
redirections are involved):

```
scrcpy --tunnel-port=1234
```


##### SSH tunnel

To communicate with a remote _adb server_ securely, it is preferable to use an
SSH tunnel.

First, make sure the _adb server_ is running on the remote computer:

```bash
adb start-server
```

Then, establish an SSH tunnel:

```bash
# local  5038 --> remote  5037
# local 27183 <-- remote 27183
ssh -CN -L5038:localhost:5037 -R27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal, run `scrcpy`:

```bash
# in bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:localhost:5038'
scrcpy
```

To avoid enabling remote port forwarding, you could force a forward connection
instead (notice the `-L` instead of `-R`):

```bash
# local  5038 --> remote  5037
# local 27183 --> remote 27183
ssh -CN -L5038:localhost:5037 -L27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal, run `scrcpy`:

```bash
# in bash
export ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

```cmd
:: in cmd
set ADB_SERVER_SOCKET=tcp:localhost:5038
scrcpy --force-adb-forward
```

```powershell
# in PowerShell
$env:ADB_SERVER_SOCKET = 'tcp:localhost:5038'
scrcpy --force-adb-forward
```


Like for wireless connections, it may be useful to reduce quality:

```
scrcpy -b2M -m800 --max-fps=15
```

### Window configuration

#### Title

By default, the window title is the device model. It can be changed:

```bash
scrcpy --window-title='My device'
```

#### Position and size

The initial window position and size may be specified:

```bash
scrcpy --window-x=100 --window-y=100 --window-width=800 --window-height=600
```

#### Borderless

To disable window decorations:

```bash
scrcpy --window-borderless
```

#### Always on top

To keep the _scrcpy_ window always on top:

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
scrcpy --rotation=1
```

Possible values:
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
scrcpy --display=1
```

The list of display ids can be retrieved by:

```bash
adb shell dumpsys display   # search "mDisplayId=" in the output
```

The secondary display may only be controlled if the device runs at least Android
10 (otherwise it is mirrored as read-only).


#### Stay awake

To prevent the device from sleeping after a delay when the device is plugged in:

```bash
scrcpy --stay-awake
scrcpy -w
```

The initial state is restored when _scrcpy_ is closed.


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
`POWER` is sent via _scrcpy_ (via right-click or <kbd>MOD</kbd>+<kbd>p</kbd>),
it will force to turn the screen off after a small delay (on a best effort
basis).  The physical `POWER` button will still cause the screen to be turned
on.

It can also be useful to prevent the device from sleeping:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```

#### Power off on close

To turn the device screen off when closing _scrcpy_:

```bash
scrcpy --power-off-on-close
```

#### Power on on start

By default, on start, the device is powered on.

To prevent this behavior:

```bash
scrcpy --no-power-on
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

Note that it only shows _physical_ touches (by a finger on the device).


#### Disable screensaver

By default, _scrcpy_ does not prevent the screensaver from running on the
computer.

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

In addition, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> injects the computer
clipboard text as a sequence of key events. This is useful when the component
does not accept text pasting (for example in _Termux_), but it can break
non-ASCII content.

**WARNING:** Pasting the computer clipboard to the device (either via
<kbd>Ctrl</kbd>+<kbd>v</kbd> or <kbd>MOD</kbd>+<kbd>v</kbd>) copies the content
into the Android clipboard. As a consequence, any Android application could read
its content. You should avoid pasting sensitive content (like passwords) that
way.

Some Android devices do not behave as expected when setting the device clipboard
programmatically. An option `--legacy-paste` is provided to change the behavior
of <kbd>Ctrl</kbd>+<kbd>v</kbd> and <kbd>MOD</kbd>+<kbd>v</kbd> so that they
also inject the computer clipboard text as a sequence of key events (the same
way as <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>).

To disable automatic clipboard synchronization, use
`--no-clipboard-autosync`.

#### Pinch-to-zoom

To simulate "pinch-to-zoom": <kbd>Ctrl</kbd>+_click-and-move_.

More precisely, hold down <kbd>Ctrl</kbd> while pressing the left-click button.
Until the left-click button is released, all mouse movements scale and rotate
the content (if supported by the app) relative to the center of the screen.

Technically, _scrcpy_ generates additional touch events from a "virtual finger"
at a location inverted through the center of the screen.

#### Physical keyboard simulation (HID)

By default, _scrcpy_ uses Android key or text injection: it works everywhere,
but is limited to ASCII.

Alternatively, `scrcpy` can simulate a physical USB keyboard on Android to
provide a better input experience (using [USB HID over AOAv2][hid-aoav2]): the
virtual keyboard is disabled and it works for all characters and IME.

[hid-aoav2]: https://source.android.com/devices/accessories/aoa2#hid-support

However, it only works if the device is connected via USB.

Note: On Windows, it may only work in [OTG mode](#otg), not while mirroring (it
is not possible to open a USB device if it is already open by another process
like the _adb daemon_).

To enable this mode:

```bash
scrcpy --hid-keyboard
scrcpy -K  # short version
```

If it fails for some reason (for example because the device is not connected via
USB), it automatically fallbacks to the default mode (with a log in the
console). This allows using the same command line options when connected over
USB and TCP/IP.

In this mode, raw key events (scancodes) are sent to the device, independently
of the host key mapping. Therefore, if your keyboard layout does not match, it
must be configured on the Android device, in Settings → System → Languages and
input → [Physical keyboard].

This settings page can be started directly:

```bash
adb shell am start -a android.settings.HARD_KEYBOARD_SETTINGS
```

However, the option is only available when the HID keyboard is enabled (or when
a physical keyboard is connected).

[Physical keyboard]: https://github.com/Genymobile/scrcpy/pull/2632#issuecomment-923756915

#### Physical mouse simulation (HID)

Similarly to the physical keyboard simulation, it is possible to simulate a
physical mouse. Likewise, it only works if the device is connected by USB.

By default, _scrcpy_ uses Android mouse events injection with absolute
coordinates. By simulating a physical mouse, a mouse pointer appears on the
Android device, and relative mouse motion, clicks and scrolls are injected.

To enable this mode:

```bash
scrcpy --hid-mouse
scrcpy -M  # short version
```

You can also add `--forward-all-clicks` to [forward all mouse
buttons][forward_all_clicks].

[forward_all_clicks]: #right-click-and-middle-click

When this mode is enabled, the computer mouse is "captured" (the mouse pointer
disappears from the computer and appears on the Android device instead).

Special capture keys, either <kbd>Alt</kbd> or <kbd>Super</kbd>, toggle
(disable or enable) the mouse capture. Use one of them to give the control of
the mouse back to the computer.


#### OTG

It is possible to run _scrcpy_ with only physical keyboard and mouse simulation
(HID), as if the computer keyboard and mouse were plugged directly to the device
via an OTG cable.

In this mode, `adb` (USB debugging) is not necessary, and mirroring is disabled.

To enable OTG mode:

```bash
scrcpy --otg
# Pass the serial if several USB devices are available
scrcpy --otg -s 0123456789abcdef
```

It is possible to enable only HID keyboard or HID mouse:

```bash
scrcpy --otg --hid-keyboard              # keyboard only
scrcpy --otg --hid-mouse                 # mouse only
scrcpy --otg --hid-keyboard --hid-mouse  # keyboard and mouse
# for convenience, enable both by default
scrcpy --otg                             # keyboard and mouse
```

Like `--hid-keyboard` and `--hid-mouse`, it only works if the device is
connected by USB.


#### Text injection preference

Two kinds of [events][textevents] are generated when typing text:
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

On the contrary, you could force to always inject raw key events:

```bash
scrcpy --raw-key-events
```

These options have no effect on HID keyboard (all key events are sent as
scancodes in this mode).

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Key repeat

By default, holding a key down generates repeated key events. This can cause
performance problems in some games, where these events are useless anyway.

To avoid forwarding repeated key events:

```bash
scrcpy --no-key-repeat
```

This option has no effect on HID keyboard (key repeat is handled by Android
directly in this mode).


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

To push a file to `/sdcard/Download/` on the device, drag & drop a (non-APK)
file to the _scrcpy_ window.

There is no visual feedback, a log is printed to the console.

The target directory can be changed on start:

```bash
scrcpy --push-target=/sdcard/Movies/
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
 | Resize window to remove black borders       | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Double-left-click¹_
 | Click on `HOME`                             | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Middle-click_
 | Click on `BACK`                             | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Right-click²_
 | Click on `APP_SWITCH`                       | <kbd>MOD</kbd>+<kbd>s</kbd> \| _4th-click³_
 | Click on `MENU` (unlock screen)⁴            | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Click on `VOLUME_UP`                        | <kbd>MOD</kbd>+<kbd>↑</kbd> _(up)_
 | Click on `VOLUME_DOWN`                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(down)_
 | Click on `POWER`                            | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Power on                                    | _Right-click²_
 | Turn device screen off (keep mirroring)     | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Turn device screen on                       | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Rotate device screen                        | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Expand notification panel                   | <kbd>MOD</kbd>+<kbd>n</kbd> \| _5th-click³_
 | Expand settings panel                       | <kbd>MOD</kbd>+<kbd>n</kbd>+<kbd>n</kbd> \| _Double-5th-click³_
 | Collapse panels                             | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copy to clipboard⁵                          | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Cut to clipboard⁵                           | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Synchronize clipboards and paste⁵           | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Inject computer clipboard text              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Enable/disable FPS counter (on stdout)      | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pinch-to-zoom                               | <kbd>Ctrl</kbd>+_click-and-move_
 | Drag & drop APK file                        | Install APK from computer
 | Drag & drop non-APK file                    | [Push file to device](#push-file-to-device)

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._  
_³4th and 5th mouse buttons, if your mouse has them._  
_⁴For react-native apps in development, `MENU` triggers development menu._  
_⁵Only on Android >= 7._

Shortcuts with repeated keys are executed by releasing and pressing the key a
second time. For example, to execute "Expand settings panel":

 1. Press and keep pressing <kbd>MOD</kbd>.
 2. Then double-press <kbd>n</kbd>.
 3. Finally, release <kbd>MOD</kbd>.

All <kbd>Ctrl</kbd>+_key_ shortcuts are forwarded to the device, so they are
handled by the active application.


## Custom paths

To use a specific `adb` binary, configure its path in the environment variable
`ADB`:

```bash
ADB=/path/to/adb scrcpy
```

To override the path of the `scrcpy-server` file, configure its path in
`SCRCPY_SERVER_PATH`.

To override the icon, configure its path in `SCRCPY_ICON_PATH`.


## Why the name _scrcpy_?

A colleague challenged me to find a name as unpronounceable as [gnirehtet].

[`strcpy`] copies a **str**ing; `scrcpy` copies a **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## How to build?

See [BUILD].


## Common issues

See the [FAQ].

[FAQ]: FAQ.md


## Developers

Read the [developers page].

[developers page]: DEVELOP.md


## Licence

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2022 Romain Vimont

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

## Contact

If you encounter a bug, please read the [FAQ] first, then open an [issue].

[issue]: https://github.com/Genymobile/scrcpy/issues

For general questions or discussions, you can also use:

 - Reddit: [`r/scrcpy`](https://www.reddit.com/r/scrcpy)
 - Twitter: [`@scrcpy_app`](https://twitter.com/scrcpy_app)

## Translations

Translations of this README in other languages are available in the [wiki].

[wiki]: https://github.com/Genymobile/scrcpy/wiki

Only this README file is guaranteed to be up-to-date.
