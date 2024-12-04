# On macOS

## Install

### From the official release

Download a static build of the [latest release]:

 - [`scrcpy-macos-aarch64-v3.0.2.tar.gz`][direct-macos-aarch64] (aarch64)  
   <sub>SHA-256: `811ba2f4e856146bdd161e24c3490d78efbec2339ca783fac791d041c0aecfb6`</sub>

 - [`scrcpy-macos-x86_64-v3.0.2.tar.gz`][direct-macos-x86_64] (x86_64)  
   <sub>SHA-256: `8effff54dca3a3e46eaaec242771a13a7f81af2e18670b3d0d8ed6b461bb4f79`</sub>

[latest release]: https://github.com/Genymobile/scrcpy/releases/latest
[direct-macos-aarch64]: https://github.com/Genymobile/scrcpy/releases/download/v3.0.2/scrcpy-macos-aarch64-v3.0.2.tar.gz
[direct-macos-x86_64]: https://github.com/Genymobile/scrcpy/releases/download/v3.0.2/scrcpy-macos-x86_64-v3.0.2.tar.gz

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
