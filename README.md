# scrcpy

This application provides display and control of Android devices connected on
USB. It does not require any _root_ access. It works on _GNU/Linux_, _Windows_
and _MacOS_.

![screenshot](assets/screenshot-debian-600.jpg)


## Requirements

The Android part requires at least API 21 (Android 5.0).

You need [adb]. It is available in the [Android SDK platform
tools][platform-tools], or packaged in your distribution (`android-adb-tools`).

On Windows, if you use the [prebuilt application](#windows), it is already
included. Otherwise, just download the [platform-tools][platform-tools-windows]
and extract the following files to a directory accessible from your `PATH`:
 - `adb.exe`
 - `AdbWinApi.dll`
 - `AdbWinUsbApi.dll`

Make sure you [enabled adb debugging][enable-adb] on your device(s).

The client requires [FFmpeg] and [LibSDL2].

[adb]: https://developer.android.com/studio/command-line/adb.html
[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling
[platform-tools]: https://developer.android.com/studio/releases/platform-tools.html
[platform-tools-windows]: https://dl.google.com/android/repository/platform-tools-latest-windows.zip
[ffmpeg]: https://en.wikipedia.org/wiki/FFmpeg
[LibSDL2]: https://en.wikipedia.org/wiki/Simple_DirectMedia_Layer

## Build and install

### System-specific steps

#### Linux

Install the required packages from your package manager.

##### Debian/Ubuntu

```bash
# runtime dependencies
sudo apt install ffmpeg libsdl2-2.0.0

# client build dependencies
sudo apt install make gcc pkg-config meson \
                 libavcodec-dev libavformat-dev libavutil-dev \
                 libsdl2-dev

# server build dependencies
sudo apt install openjdk-8-jdk
```

##### Fedora

```bash
# enable RPM fusion free
sudo dnf install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm

# client build dependencies
sudo dnf install SDL2-devel ffms2-devel meson gcc make

# server build dependencies
sudo dnf install java
```

##### Arch Linux

Two [AUR] packages have been created by users:

 - [`scrcpy`](https://aur.archlinux.org/packages/scrcpy/)
 - [`scrcpy-prebuiltserver`](https://aur.archlinux.org/packages/scrcpy-prebuiltserver/)

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository


#### Windows

For Windows, for simplicity, a prebuilt archive with all the dependencies
(including `adb`) is available:

 - [`scrcpy-windows-with-deps-v1.1.zip`][direct-windows-with-deps].  
   _(SHA-256: 27eb36c15937601d1062c1dc0b45faae0e06fefea2019aadeb4fa7f76a07bb4c)_

[direct-windows-with-deps]: https://github.com/Genymobile/scrcpy/releases/download/v1.1/scrcpy-windows-with-deps-v1.1.zip

_(It's just a portable version including _dll_ copied from MSYS2.)_

Instead, you may want to build it manually. You need [MSYS2] to build the
project. From an MSYS2 terminal, install the required packages:

[MSYS2]: http://www.msys2.org/

```bash
# runtime dependencies
pacman -S mingw-w64-x86_64-SDL2 \
          mingw-w64-x86_64-ffmpeg

# client build dependencies
pacman -S mingw-w64-x86_64-make \
          mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-pkg-config \
          mingw-w64-x86_64-meson
```

Java (>= 7) is not available in MSYS2, so if you plan to build the server,
install it manually and make it available from the `PATH`:

```bash
export PATH="$JAVA_HOME/bin:$PATH"
```

#### Mac OS

The application is available in [Homebrew]. Just install it:

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

Instead, you may want to build it manually. Install the packages:


```bash
# runtime dependencies
brew install sdl2 ffmpeg

# client build dependencies
brew install pkg-config meson
```

Additionally, if you want to build the server, install Java 8 from Caskroom, and
make it avaliable from the `PATH`:

```bash
brew tap caskroom/versions
brew cask install java8
export JAVA_HOME="$(/usr/libexec/java_home --version 1.8)"
export PATH="$JAVA_HOME/bin:$PATH"
```

#### Docker

See [pierlon/scrcpy-docker](https://github.com/pierlon/scrcpy-docker).

### Common steps

Install the [Android SDK] (_Android Studio_), and set `ANDROID_HOME` to
its directory. For example:

[Android SDK]: https://developer.android.com/studio/index.html

```bash
export ANDROID_HOME=~/android/sdk
```

Clone the project:

```bash
git clone https://github.com/Genymobile/scrcpy
cd scrcpy
```

Then, build:

```bash
meson x --buildtype release --strip -Db_lto=true
cd x
ninja
```

You can test it from here:

```bash
ninja run
```

Or you can install it on the system:

```bash
sudo ninja install    # without sudo on Windows
```

This installs two files:

 - `/usr/local/bin/scrcpy`
 - `/usr/local/share/scrcpy/scrcpy-server.jar`

Just remove them to "uninstall" the application.


#### Prebuilt server

Since the server binary, that will be pushed to the Android device, does not
depend on your system and architecture, you may want to use the prebuilt binary
instead:

 - [`scrcpy-server-v1.1.jar`][direct-scrcpy-server].  
   _(SHA-256: 14826512bf38447ec94adf3b531676ce038d19e7e06757fb4e537882b17e77b3)_

[direct-scrcpy-server]: https://github.com/Genymobile/scrcpy/releases/download/v1.1/scrcpy-server-v1.1.jar

In that case, the build does not require Java or the Android SDK.

Download the prebuilt server somewhere, and specify its path during the Meson
configuration:

```bash
meson x --buildtype release --strip -Db_lto=true \
    -Dprebuilt_server=/path/to/scrcpy-server.jar
cd x
ninja
sudo ninja install
```

## Run

_At runtime, `adb` must be accessible from your `PATH`._

If everything is ok, just plug an Android device, and execute:

```bash
scrcpy
```

It accepts command-line arguments, listed by:

```bash
scrcpy --help
```

For example, to decrease video bitrate to 2Mbps (default is 8Mbps):

```bash
scrcpy -b 2M
```

To limit the video dimensions (e.g. if the device is 2540×1440, but the host
screen is smaller, or cannot decode such a high definition):

```bash
scrcpy -m 1024
```

If several devices are listed in `adb devices`, you must specify the _serial_:

```bash
scrcpy -s 0123456789abcdef
```

To run without installing:

```bash
./run x [options]
```

(where `x` is your build directory).


## Shortcuts

 | Action                                 |   Shortcut                    |
 | -------------------------------------- |:----------------------------  |
 | switch fullscreen mode                 | `Ctrl`+`f`                    |
 | resize window to 1:1 (pixel-perfect)   | `Ctrl`+`g`                    |
 | resize window to remove black borders  | `Ctrl`+`x` \| _Double-click¹_ |
 | click on `HOME`                        | `Ctrl`+`h` \| _Middle-click_  |
 | click on `BACK`                        | `Ctrl`+`b` \| _Right-click²_  |
 | click on `APP_SWITCH`                  | `Ctrl`+`m`                    |
 | click on `VOLUME_UP`                   | `Ctrl`+`+`                    |
 | click on `VOLUME_DOWN`                 | `Ctrl`+`-`                    |
 | click on `POWER`                       | `Ctrl`+`p`                    |
 | turn screen on                         | _Right-click²_                |
 | paste computer clipboard to device     | `Ctrl`+`v`                    |
 | enable/disable FPS counter (on stdout) | `Ctrl`+`i`                    |

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._


## Why _scrcpy_?

A colleague challenged me to find a name as unpronounceable as [gnirehtet].

[`strcpy`] copies a **str**ing; `scrcpy` copies a **scr**een.

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## Common issues

See the [FAQ](FAQ.md).


## Developers

Read the [developers page].

[developers page]: DEVELOP.md


## Licence

    Copyright (C) 2018 Genymobile

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

## Articles

- [Introducing scrcpy](https://blog.rom1v.com/2018/03/introducing-scrcpy/)
- [Scrcpy now works wirelessly](https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/)
