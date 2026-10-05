# On Windows

## Install

### From the official release

Download the [latest release]:

 - [`scrcpy-win64-v5.0.zip`][direct-win64] (64-bit)  
   <sub>SHA-256: `44c10d9e82f20ea67227d14d37bf9fbe3603117c5736df3f514544a02ba20a73`</sub>
 - [`scrcpy-win32-v5.0.zip`][direct-win32] (32-bit)  
   <sub>SHA-256: `31413dd59a80cb76e7747ef91b4994934b546c8860e8e7976046c612a50e5cc3`</sub>
 - [`scrcpy-winarm64-v5.0.zip`][direct-win32] (ARM 64-bit)  
   <sub>SHA-256: `afc1db17b216f91470d2c8f41ea786ae18ab1cb2bc2324bc05ec3d7685aa6054`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v5.0/scrcpy-win64-v5.0.zip
[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v5.0/scrcpy-win32-v5.0.zip
[direct-winarm64]: https://github.com/Genymobile/scrcpy/releases/download/v5.0/scrcpy-winarm64-v5.0.zip

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
