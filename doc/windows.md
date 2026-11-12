# On Windows

## Install

### From the official release

Download the [latest release]:

 - [`scrcpy-win64-v5.0.1.zip`][direct-win64] (64-bit)  
   <sub>SHA-256: `b12a2c4ee8be317422451fc7dcf8ee20a71b5ea7ef9ad73ddd825a25316227a5`</sub>
 - [`scrcpy-win32-v5.0.1.zip`][direct-win32] (32-bit)  
   <sub>SHA-256: `0cb65ebf1bce892fa4d1502d699af0c7a1a7f24d7febcb9ef72894e3f23fc2ad`</sub>
 - [`scrcpy-winarm64-v5.0.1.zip`][direct-win32] (ARM 64-bit)  
   <sub>SHA-256: `16c3fc2068df64946f670e3504b4752073ebae47a416a45b46e79a5ab19c689b`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v5.0.1/scrcpy-win64-v5.0.1.zip
[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v5.0.1/scrcpy-win32-v5.0.1.zip
[direct-winarm64]: https://github.com/Genymobile/scrcpy/releases/download/v5.0.1/scrcpy-winarm64-v5.0.1.zip

and extract it.


### From a package manager

From [WinGet] (ADB and other dependencies will be installed alongside scrcpy):

```bash
winget install --exact Genymobile.scrcpy
```

From [Chocolatey]:

```bash
choco install scrcpy
choco install adb    # if you don't have it yet
```

From [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # if you don't have it yet
```

[WinGet]: https://github.com/microsoft/winget-cli
[Chocolatey]: https://chocolatey.org/
[Scoop]: https://scoop.sh

_See [build.md](build.md) to build and install the app manually._


## Run

_Make sure that your device meets the [prerequisites](/README.md#prerequisites)._

Scrcpy is a command line application: it is mainly intended to be executed from
a terminal with command line arguments.

To open a terminal at the expected location, double-click on
`open_a_terminal_here.bat` in your scrcpy directory, then type your command. For
example, without arguments:

```bash
scrcpy
```

or with arguments (here to disable audio and record to `file.mkv`):

```bash
scrcpy --no-audio --record=file.mkv
```

Documentation for command line arguments is available:
 - `scrcpy --help`
 - on [github](/README.md)

If you plan to always use the same arguments, create a file `myscrcpy.bat`
(enable [show file extensions] to avoid confusion) containing your command, For
example:

```bash
scrcpy --prefer-text --turn-screen-off --stay-awake
```

Add `--pause-on-exit=if-error` if you want the console to remain open when
scrcpy fails:

```bash
scrcpy --prefer-text --turn-screen-off --stay-awake --pause-on-exit=if-error
```

[show file extensions]: https://www.howtogeek.com/205086/beginner-how-to-make-windows-show-file-extensions/

Then just double-click on that file to run it.

To start scrcpy without opening a terminal, double-click `scrcpy-noconsole.vbs`
(note that errors won't be shown). To pass arguments, edit (a copy of)
`scrcpy-noconsole.vbs` and add the desired arguments.

## Webcam

On Windows, it is possible to send the video stream to the [OBS Virtual
Camera], so that the Android device can be opened like a webcam by any
application supporting DirectShow cameras (browsers, video conference tools…).

[OBS Virtual Camera]: https://obsproject.com/kb/virtual-camera-guide

[OBS Studio] (version 26.1 or later) must be installed, since it provides the
virtual camera driver. OBS itself must _not_ be running (its virtual camera
would conflict with scrcpy).

[OBS Studio]: https://obsproject.com/

```bash
scrcpy --obs-virtualcam
scrcpy --obs-virtualcam --no-video-playback  # disable playback window
scrcpy --video-source=camera --camera-size=1920x1080 --obs-virtualcam --no-playback
```

Then select the "OBS Virtual Camera" device in the target application. While
scrcpy is not running, the virtual camera shows a placeholder image.

The camera keeps the format it was opened with: if the video size changes
(for example on device rotation), the frames are scaled to that format. To
avoid this, lock the orientation (see [`--capture-orientation`](video.md#orientation)).
