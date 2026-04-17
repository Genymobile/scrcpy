# On Windows

## Install

### From the official release

Download the [latest release]:

 - [`scrcpy-win64-v3.3.4.zip`][direct-win64] (64-bit)  
   <sub>SHA-256: `d8a155b7c180b7ca4cdadd40712b8750b63f3aab48cb5b8a2a39ac2d0d4c5d38`</sub>
 - [`scrcpy-win32-v3.3.4.zip`][direct-win32] (32-bit)  
   <sub>SHA-256: `393f7d5379dabd8aacc41184755c3d0df975cd2861353cb7a8d50e0835e2eb72`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.4/scrcpy-win64-v3.3.4.zip
[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.4/scrcpy-win32-v3.3.4.zip

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

## Add scrcpy to PATH

First we need to locate the executable. Then create a shortcut for it or add it to PATH.

### If installed with WinGet
Open the Run dialog (<kbd>Win+R</kbd>) and type `%USERPROFILE%\AppData\Local\Microsoft\WinGet\Packages\Genymobile.scrcpy_Microsoft.Winget.Source_8wekyb3d8bbwe`, (press <kbd>Enter</kbd>) and double click the "scrcpy-win64-v3.3.3" folder (or the corresponding version you have), you should see a `scrcpy.exe` file inside. 

If you don't plan to execute it with arguments you could just create a Desktop shortcut for the executable. If you want to use the CLI, you can add the executable to user PATH via CLI with:

```bat
setx PATH "%PATH%;%USERPROFILE%\AppData\Local\Microsoft\WinGet\Packages\Genymobile.scrcpy_Microsoft.Winget.Source_8wekyb3d8bbwe\scrcpy-win64-v3.3.3"
```
_Don't forget to use the actual numbering from the version you are installing (v3.3.3 in this example)._

You can also add it to (user or system) PATH manually via 'Edit Environment Variables'.

## Connect the Android device
_Make sure that your device meets the [prerequisites](/README.md#prerequisites)._

Connect your device via USB and accept the connection with any mode. Both "Charging Only" and "File Transfer / Android Auto" should work. You can also use any other mode, the important part is to accept the following dialog ("Allow USB debugging?") for enabling ADB (Android Debug Bridge) between the computer and the device. 


## Run

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
