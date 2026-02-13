# On macOS

## Install

### From the official release

Download a static build of the [latest release]:

 - [`scrcpy-macos-aarch64-v3.3.4.tar.gz`][direct-macos-aarch64] (aarch64)  
   <sub>SHA-256: `8fef43520405dd523c74e1530ac68febcc5a405ea89712c874936675da8513dd`</sub>
 - [`scrcpy-macos-x86_64-v3.3.4.tar.gz`][direct-macos-x86_64] (x86_64)  
   <sub>SHA-256: `cf9b3453a33279b6009dfb256b1a84c374bd4c30a71edd74bacab28d72a5d929`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-macos-aarch64]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.4/scrcpy-macos-aarch64-v3.3.4.tar.gz
[direct-macos-x86_64]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.4/scrcpy-macos-x86_64-v3.3.4.tar.gz

and extract it.

_Static builds of scrcpy for macOS are still experimental._


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
