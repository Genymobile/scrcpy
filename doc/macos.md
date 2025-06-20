# On macOS

## Install

### From the official release

Download a static build of the [latest release]:

 - [`scrcpy-macos-aarch64-v3.3.1.tar.gz`][direct-macos-aarch64] (aarch64)  
   <sub>SHA-256: `907b925900ebd8499c1e47acc9689a95bd3a6f9930eb1d7bdfbca8375ae4f139`</sub>

 - [`scrcpy-macos-x86_64-v3.3.1.tar.gz`][direct-macos-x86_64] (x86_64)  
   <sub>SHA-256: `69772491dad718eea82fc65c8e89febff7d41c4ce6faff02f4789a588d10fd7d`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-macos-aarch64]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.1/scrcpy-macos-aarch64-v3.3.1.tar.gz
[direct-macos-x86_64]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.1/scrcpy-macos-x86_64-v3.3.1.tar.gz

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
