# Build scrcpy

Here are the instructions to build _scrcpy_ (client and server).

If you just want to build and install the latest release, follow the simplified
process described in [doc/linux.md](linux.md).

## Branches

There are two main branches:
 - `master`: contains the latest release. It is the home page of the project on
   GitHub.
 - `dev`: the current development branch. Every commit present in `dev` will be
   in the next release.

If you want to contribute code, please base your commits on the latest `dev`
branch.


## Requirements

You need [adb]. It is available in the [Android SDK platform
tools][platform-tools], or packaged in your distribution (`adb`).

On Windows, download the [platform-tools][platform-tools-windows] and extract
the following files to a directory accessible from your `PATH`:
 - `adb.exe`
 - `AdbWinApi.dll`
 - `AdbWinUsbApi.dll`

It is also available in scrcpy releases.

The client requires [FFmpeg] and [LibSDL2]. Just follow the instructions.

[adb]: https://developer.android.com/studio/command-line/adb.html
[platform-tools]: https://developer.android.com/studio/releases/platform-tools.html
[platform-tools-windows]: https://dl.google.com/android/repository/platform-tools-latest-windows.zip
[ffmpeg]: https://en.wikipedia.org/wiki/FFmpeg
[LibSDL2]: https://en.wikipedia.org/wiki/Simple_DirectMedia_Layer



## System-specific steps

### Linux

Install the required packages from your package manager.

#### Debian/Ubuntu

```bash
# runtime dependencies
sudo apt install ffmpeg libsdl2-2.0-0 adb libusb-1.0-0

# client build dependencies
sudo apt install gcc git pkg-config meson ninja-build libsdl2-dev \
                 libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev \
                 libswresample-dev libusb-1.0-0-dev

# server build dependencies
sudo apt install openjdk-17-jdk
```

On old versions (like Ubuntu 16.04), `meson` is too old. In that case, install
it from `pip3`:

```bash
sudo apt install python3-pip
pip3 install meson
```


#### Fedora

```bash
# enable RPM fusion free
sudo dnf install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm

# client build dependencies
sudo dnf install SDL2-devel ffms2-devel libusb1-devel libavdevice-free-devel meson gcc make

# server build dependencies
sudo dnf install java-devel
```



### Windows

#### Cross-compile from Linux

This is the preferred method (and the way the release is built).

From _Debian_, install _mingw_:

```bash
sudo apt install mingw-w64 mingw-w64-tools libz-mingw-w64-dev
```

You also need the JDK to build the server:

```bash
sudo apt install openjdk-17-jdk
```

Then generate the releases:

```bash
./release.sh
```

It will generate win32 and win64 releases into `dist/`.


#### In MSYS2

From Windows, you need [MSYS2] to build the project. From an MSYS2 terminal,
install the required packages:

[MSYS2]: http://www.msys2.org/

```bash
# runtime dependencies
pacman -S mingw-w64-x86_64-SDL2 \
          mingw-w64-x86_64-ffmpeg \
          mingw-w64-x86_64-libusb

# client build dependencies
pacman -S mingw-w64-x86_64-make \
          mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-pkg-config \
          mingw-w64-x86_64-meson
```

For a 32 bits version, replace `x86_64` by `i686`:

```bash
# runtime dependencies
pacman -S mingw-w64-i686-SDL2 \
          mingw-w64-i686-ffmpeg \
          mingw-w64-i686-libusb

# client build dependencies
pacman -S mingw-w64-i686-make \
          mingw-w64-i686-gcc \
          mingw-w64-i686-pkg-config \
          mingw-w64-i686-meson
```

Java (>= 7) is not available in MSYS2, so if you plan to build the server,
install it manually and make it available from the `PATH`:

```bash
export PATH="$JAVA_HOME/bin:$PATH"
```

### Mac OS

Install the packages with [Homebrew]:

[Homebrew]: https://brew.sh/

```bash
# runtime dependencies
brew install sdl2 ffmpeg libusb

# client build dependencies
brew install pkg-config meson
```

Additionally, if you want to build the server, install Java 17 from Caskroom, and
make it available from the `PATH`:

```bash
brew tap homebrew/cask-versions
brew install adoptopenjdk/openjdk/adoptopenjdk17
export JAVA_HOME="$(/usr/libexec/java_home --version 1.17)"
export PATH="$JAVA_HOME/bin:$PATH"
```

### Docker

See [pierlon/scrcpy-docker](https://github.com/pierlon/scrcpy-docker).


## Common steps

**As a non-root user**, clone the project:

```bash
git clone https://github.com/Genymobile/scrcpy
cd scrcpy
```


### Build

You may want to build only the client: the server binary, which will be pushed
to the Android device, does not depend on your system and architecture. In that
case, use the [prebuilt server] (so you will not need Java or the Android SDK).

[prebuilt server]: #option-2-use-prebuilt-server


#### Option 1: Build everything from sources

Install the [Android SDK] (_Android Studio_), and set `ANDROID_SDK_ROOT` to its
directory. For example:

[Android SDK]: https://developer.android.com/studio/index.html

```bash
# Linux
export ANDROID_SDK_ROOT=~/Android/Sdk
# Mac
export ANDROID_SDK_ROOT=~/Library/Android/sdk
# Windows
set ANDROID_SDK_ROOT=%LOCALAPPDATA%\Android\sdk
```

Then, build:

```bash
meson setup x --buildtype=release --strip -Db_lto=true
ninja -Cx  # DO NOT RUN AS ROOT
```

_Note: `ninja` [must][ninja-user] be run as a non-root user (only `ninja
install` must be run as root)._

[ninja-user]: https://github.com/Genymobile/scrcpy/commit/4c49b27e9f6be02b8e63b508b60535426bd0291a


#### Option 2: Use prebuilt server

 - [`scrcpy-server-v3.3.3`][direct-scrcpy-server]  
   <sub>SHA-256: `7e70323ba7f259649dd4acce97ac4fefbae8102b2c6d91e2e7be613fd5354be0`</sub>

[direct-scrcpy-server]: https://github.com/Genymobile/scrcpy/releases/download/v3.3.3/scrcpy-server-v3.3.3

Download the prebuilt server somewhere, and specify its path during the Meson
configuration:

```bash
meson setup x --buildtype=release --strip -Db_lto=true \
    -Dprebuilt_server=/path/to/scrcpy-server
ninja -Cx  # DO NOT RUN AS ROOT
```

The server only works with a matching client version (this server works with the
`master` branch).


### Run without installing:

```bash
./run x [options]
```


### Install

After a successful build, you can install _scrcpy_ on the system:

```bash
sudo ninja -Cx install    # without sudo on Windows
```

This installs several files:

 - `/usr/local/bin/scrcpy` (main app)
 - `/usr/local/share/scrcpy/scrcpy-server` (server to push to the device)
 - `/usr/local/share/man/man1/scrcpy.1` (manpage)
 - `/usr/local/share/icons/hicolor/256x256/apps/icon.png` (app icon)
 - `/usr/local/share/zsh/site-functions/_scrcpy` (zsh completion)
 - `/usr/local/share/bash-completion/completions/scrcpy` (bash completion)

You can then run `scrcpy`.


### Uninstall

```bash
sudo ninja -Cx uninstall  # without sudo on Windows
```
