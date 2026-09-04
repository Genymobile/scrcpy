# On Windows

## Install

### From the official release

Download the [latest release]:

 - [`slink-win64-v4.1.zip`][direct-win64] (64-bit)  
   <sub>SHA-256: `5b12172b3264b2889f4583ee64752ce832e29bc8b1089dca81093459697165db`</sub>
 - [`slink-win32-v4.1.zip`][direct-win32] (32-bit)  
   <sub>SHA-256: `fa57b36622a53b6aec74c5e5b5c08236165efa445c4f186d48f176ebf9c24eec`</sub>

[latest release]: https://github.com/Genymobile/slink/releases/latest
[direct-win64]: https://github.com/Genymobile/slink/releases/download/v4.1/slink-win64-v4.1.zip
[direct-win32]: https://github.com/Genymobile/slink/releases/download/v4.1/slink-win32-v4.1.zip

and extract it.


### From a package manager

From [WinGet] (ADB and other dependencies will be installed alongside slink):

```bash
winget install --exact Genymobile.slink
```

From [Chocolatey]:

```bash
choco install slink
choco install adb    # if you don't have it yet
```

From [Scoop]:

```bash
scoop install slink
scoop install adb    # if you don't have it yet
```

[WinGet]: https://github.com/microsoft/winget-cli
[Chocolatey]: https://chocolatey.org/
[Scoop]: https://scoop.sh

_See [build.md](build.md) to build and install the app manually._


## Run

_Make sure that your device meets the [prerequisites](/README.md#prerequisites)._

slink is a command line application: it is mainly intended to be executed from
a terminal with command line arguments.

To open a terminal at the expected location, double-click on
`open_a_terminal_here.bat` in your slink directory, then type your command. For
example, without arguments:

```bash
slink
```

or with arguments (here to disable audio and record to `file.mkv`):

```bash
slink --no-audio --record=file.mkv
```

Documentation for command line arguments is available:
 - `slink --help`
 - on [github](/README.md)

If you plan to always use the same arguments, create a file `myslink.bat`
(enable [show file extensions] to avoid confusion) containing your command, For
example:

```bash
slink --prefer-text --turn-screen-off --stay-awake
```

Add `--pause-on-exit=if-error` if you want the console to remain open when
slink fails:

```bash
slink --prefer-text --turn-screen-off --stay-awake --pause-on-exit=if-error
```

[show file extensions]: https://www.howtogeek.com/205086/beginner-how-to-make-windows-show-file-extensions/

Then just double-click on that file to run it.

To start slink without opening a terminal, double-click `slink-noconsole.vbs`
(note that errors won't be shown). To pass arguments, edit (a copy of)
`slink-noconsole.vbs` and add the desired arguments.
