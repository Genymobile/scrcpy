# On Windows

## Install

Download the [latest release]:

 - [`scrcpy-win64-v2.7.zip`][direct-win64] (64-bit)  
   <sub>SHA-256: `5910bc18d5a16f42d84185ddc7e16a4cee6a6f5f33451559c1a1d6d0099bd5f5`</sub>
 - [`scrcpy-win32-v2.7.zip`][direct-win32] (32-bit)  
   <sub>SHA-256: `ef4daf89d500f33d78b830625536ecb18481429dd94433e7634c824292059d06`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v2.7/scrcpy-win64-v2.7.zip
[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v2.7/scrcpy-win32-v2.7.zip

and extract it.

Alternatively, you could install it from packages manager, like [Chocolatey]:

```bash
choco install scrcpy
choco install adb    # if you don't have it yet
```

or [Scoop]:


```bash
scoop install scrcpy
scoop install adb    # if you don't have it yet
```

[Winget]: https://github.com/microsoft/winget-cli
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
