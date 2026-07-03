# On macOS

## Install

### From the official release

Download a static build of the [latest release]:

 - [`scrcpy-macos-aarch64-v4.0.tar.gz`][direct-macos-aarch64] (aarch64)  
   <sub>SHA-256: `f5167fe047fe4a2ae2c2ea8634c7145a4d64d0b6005f24bb45639a965b8c60d4`</sub>
 - [`scrcpy-macos-x86_64-v4.0.tar.gz`][direct-macos-x86_64] (x86_64)  
   <sub>SHA-256: `b83169f856d7022ed0e4428d98acea18dde2d63f49611b52ea137577ce4efe6b`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-macos-aarch64]: https://github.com/Genymobile/scrcpy/releases/download/v4.0/scrcpy-macos-aarch64-v4.0.tar.gz
[direct-macos-x86_64]: https://github.com/Genymobile/scrcpy/releases/download/v4.0/scrcpy-macos-x86_64-v4.0.tar.gz

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
