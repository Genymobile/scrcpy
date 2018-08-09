# scrcpy (v1.3)

This application provides display and control of Android devices connected on
USB (or [over TCP/IP][article-tcpip]). It does not require any _root_ access.
It works on _GNU/Linux_, _Windows_ and _MacOS_.

![screenshot](assets/screenshot-debian-600.jpg)


## Requirements

The Android part requires at least API 21 (Android 5.0).

You need [adb]. It is available in the [Android SDK platform
tools][platform-tools], or packaged in your distribution (`android-adb-tools`).

On Windows, just [download scrcpy for Windows](#windows), `adb` is included.

Make sure you [enabled adb debugging][enable-adb] on your device(s).

The client requires [FFmpeg] and [LibSDL2]. On Windows, they are included in the
[prebuilt application](#windows).

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

For Windows, for simplicity, prebuilt archives with all the dependencies
(including `adb`) are available:

 - [`scrcpy-win32-v1.3.zip`][direct-win32].  
   _(SHA-256: 51a2990e631ed469a7a86ff38107d517a91d313fb3f8327eb7bc71dde40870b5)_
 - [`scrcpy-win64-v1.3.zip`][direct-win64].  
   _(SHA-256: 0768a80d3d600d0bbcd220ca150ae88a3a58d1fe85c308a8c61f44480b711e43)_

[direct-win32]: https://github.com/Genymobile/scrcpy/releases/download/v1.3/scrcpy-win32-v1.3.zip
[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.3/scrcpy-win64-v1.3.zip

Instead, you may want to build it manually.

In that case, download the [platform-tools][platform-tools-windows] and extract
the following files to a directory accessible from your `PATH`:
 - `adb.exe`
 - `AdbWinApi.dll`
 - `AdbWinUsbApi.dll`

##### Cross-compile from Linux

This is the preferred method (and the way the release is built).

From _Debian_, install _mingw_:

```bash
sudo apt install mingw-w64 mingw-w64-tools
```

You also need the JDK to build the server:

```bash
sudo apt install openjdk-8-jdk
```

Then generate the releases:

```bash
make -f Makefile.CrossWindows
```

It will generate win32 and win64 releases into `dist/`.


##### In MSYS2

From Windows, you need [MSYS2] to build the project. From an MSYS2 terminal,
install the required packages:

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

For a 32 bits version, replace `x86_64` by `i686`:

```bash
# runtime dependencies
pacman -S mingw-w64-i686-SDL2 \
          mingw-w64-i686-ffmpeg

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

 - [`scrcpy-server-v1.3.jar`][direct-scrcpy-server].  
   _(SHA-256: 0f9a5a217f33f0ed7a1498ceb3c0cccf31c53533893aa952e674c1571d2740c1)_

[direct-scrcpy-server]: https://github.com/Genymobile/scrcpy/releases/download/v1.3/scrcpy-server-v1.3.jar

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

The device screen may be cropped to mirror only part of the screen:

```bash
scrcpy -c 1224:1440:0:0   # 1224x1440 at offset (0,0)
```

If several devices are listed in `adb devices`, you must specify the _serial_:

```bash
scrcpy -s 0123456789abcdef
```

To show physical touches while scrcpy is running:

```bash
scrcpy -t
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
 | click on `APP_SWITCH`                  | `Ctrl`+`s`                    |
 | click on `MENU`                        | `Ctrl`+`m`                    |
 | click on `VOLUME_UP`                   | `Ctrl`+`↑` _(up)_             |
 | click on `VOLUME_DOWN`                 | `Ctrl`+`↓` _(down)_           |
 | click on `POWER`                       | `Ctrl`+`p`                    |
 | turn screen on                         | _Right-click²_                |
 | paste computer clipboard to device     | `Ctrl`+`v`                    |
 | enable/disable FPS counter (on stdout) | `Ctrl`+`i`                    |
 | install APK from computer              | drag & drop APK file          |

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

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
