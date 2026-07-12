# On macOS

## Install

### From the official release

Download a static build of the [latest release]:

 - [`scrcpy-macos-aarch64-v4.1.tar.gz`][direct-macos-aarch64] (aarch64)  
   <sub>SHA-256: `20fd47c9014dd5e0fa77091f3cb7adbda8445a360c4584aeaa0150b5b3988ff3`</sub>
 - [`scrcpy-macos-x86_64-v4.1.tar.gz`][direct-macos-x86_64] (x86_64)  
   <sub>SHA-256: `ee2a7223bc8dbdc4f482db1134bcf441178dafb833492b71ca4c22090c58ce72`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-macos-aarch64]: https://github.com/Genymobile/scrcpy/releases/download/v4.1/scrcpy-macos-aarch64-v4.1.tar.gz
[direct-macos-x86_64]: https://github.com/Genymobile/scrcpy/releases/download/v4.1/scrcpy-macos-x86_64-v4.1.tar.gz

and extract it.


### From a package manager

Scrcpy is available in [Homebrew]:

```bash
brew install scrcpy
```

[Homebrew]: https://brew.sh/

You need `adb`, accessible from your `PATH`. If you don't have it yet:

```bash
brew install --cask android-platform-tools
```

Alternatively, Scrcpy is also available in [MacPorts], which sets up `adb` for you:

```bash
sudo port install scrcpy
```

[MacPorts]: https://www.macports.org/

_See [build.md](build.md) to build and install the app manually._


## Run

_Make sure that your device meets the [prerequisites](/README.md#prerequisites)._

Once installed, run from a terminal:

```bash
scrcpy
```

or with arguments (here to disable audio and record to `file.mkv`):

```bash
scrcpy --no-audio --record=file.mkv
```

Documentation for command line arguments is available:
 - `man scrcpy`
 - `scrcpy --help`
 - on [github](/README.md)
