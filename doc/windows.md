# On Windows

## Prerequisites

Install the Android USB drivers so `adb` will detect your device:

- [Google USB driver][usb-google],
- [Samsung USB driver][usb-samsung], or
- Your manufacturer-specific USB drivers.

[usb-google]: https://developer.android.com/studio/run/win-usb
[usb-samsung]: https://developer.samsung.com/android-usb-driver

## Install

### From the official release

Download the [latest release]:

 - [`scrcpy-win64-v3.3.2.zip`][direct-win64] (64-bit)  
   <sub>SHA-256: `8f7b19371657b872e271e6b02a0c758c61c6e31e032e9df55a83aa3aab960bfa`</sub>
 - [`scrcpy-win32-v3.3.2.zip`][direct-win32] (32-bit)  
   <sub>SHA-256: `cff2bbebdcfe14a023b77cd601fc4420b5631b19bd4b09ce4dcd4e5bf8e63244`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.2/scrcpy-win64-v3.3.2.zip
[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.2/scrcpy-win32-v3.3.2.zip

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

```
scrcpy --no-audio --record=file.mkv
```

Documentation for command line arguments is available:
 - `scrcpy --help`
 - on [github](/README.md)

To start scrcpy directly without opening a terminal, double-click on one of
these files:
 - `scrcpy-console.bat`: start with a terminal open (it will close when scrcpy
   terminates, unless an error occurs);
 - `scrcpy-noconsole.vbs`: start without a terminal (but you won't see any error
   message).

_Avoid double-clicking on `scrcpy.exe` directly: on error, the terminal would
close immediately and you won't have time to read any error message (this
executable is intended to be run from the terminal). Use `scrcpy-console.bat`
instead._

If you plan to always use the same arguments, create a file `myscrcpy.bat`
(enable [show file extensions] to avoid confusion) containing your command, For
example:

```bash
scrcpy --prefer-text --turn-screen-off --stay-awake
```

[show file extensions]: https://www.howtogeek.com/205086/beginner-how-to-make-windows-show-file-extensions/

Then just double-click on that file.

You could also edit (a copy of) `scrcpy-console.bat` or `scrcpy-noconsole.vbs`
to add some arguments.
