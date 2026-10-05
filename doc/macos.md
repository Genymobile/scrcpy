# On macOS

## Install

### From the official release

Download a static build of the [latest release]:

 - [`scrcpy-macos-aarch64-v5.0.tar.gz`][direct-macos-aarch64] (aarch64)  
   <sub>SHA-256: `7cb4e41c859b05b36e89dc9be6c353cc5980c00d7f7f6a763b5b355551b82e9c`</sub>
 - [`scrcpy-macos-x86_64-v5.0.tar.gz`][direct-macos-x86_64] (x86_64)  
   <sub>SHA-256: `dacb995c8eb42528cb96b2da2a2e3110fe5c82281378e6044fc7cccf48a7c39a`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-macos-aarch64]: https://github.com/Genymobile/scrcpy/releases/download/v5.0/scrcpy-macos-aarch64-v5.0.tar.gz
[direct-macos-x86_64]: https://github.com/Genymobile/scrcpy/releases/download/v5.0/scrcpy-macos-x86_64-v5.0.tar.gz

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
