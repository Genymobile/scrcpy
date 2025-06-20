# On Windows

## Install

### From the official release

Download the [latest release]:

 - [`scrcpy-win64-v3.3.1.zip`][direct-win64] (64-bit)  
   <sub>SHA-256: `4fcad494772a3ae5de9a133149f8856d2fc429b41795f7cf7c754e0c1bb6fbc0`</sub>
 - [`scrcpy-win32-v3.3.1.zip`][direct-win32] (32-bit)  
   <sub>SHA-256: `ccdf1b4f5d19dfe760446a107e55b0a010a00e097d46533a161499c9333a20a6`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.1/scrcpy-win64-v3.3.1.zip
[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.1/scrcpy-win32-v3.3.1.zip

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
