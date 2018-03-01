# scrcpy

This project displays screens of Android devices connected on USB, and allow to
control them using the host mouse and keyboard. It does not require any _root_
access. It works on _GNU/Linux_, _Windows_ and _Mac OS_.

![screenshot](assets/screenshot-debian-600.jpg)


## Requirements

The Android part requires at least API 21 (Android 5.0).

You need [adb] (recent enough so that `adb reverse` is implemented, it works
with 1.0.36). It is available in the [Android SDK platform
tools][platform-tools], on packaged in your distribution (`android-adb-tools`).

On Windows, just download the [platform-tools][platform-tools-windows] and
extract the following files to a directory accessible from your `PATH`:
 - `adb.exe`
 - `AdbWinApi.dll`
 - `AdbWinUsbApi.dll`

Make sure you [enabled adb debugging][enable-adb] on your device(s).

[adb]: https://developer.android.com/studio/command-line/adb.html
[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling
[platform-tools]: https://developer.android.com/studio/releases/platform-tools.html
[platform-tools-windows]: https://dl.google.com/android/repository/platform-tools-latest-windows.zip

The client requires _FFmpeg_ and _LibSDL2_.


## Build and install

### System-specific steps

#### Linux

Install the required packages from your package manager (here, for Debian):

    # runtime dependencies
    sudo apt install ffmpeg libsdl2-2.0.0

    # build dependencies
    sudo apt install make gcc openjdk-8-jdk pkg-config meson zip \
                     libavcodec-dev libavformat-dev libavutil-dev \
                     libsdl2-dev


#### Windows

For Windows, for simplicity, a prebuilt package with all the dependencies
(including `adb`) is available: TODO.

Instead, you may want to build it manually. You need [MSYS2] to build the
project. From an MSYS2 terminal, install the required packages:

[MSYS2]: http://www.msys2.org/

    # runtime dependencies
    pacman -S mingw-w64-x86_64-SDL2 \
              mingw-w64-x86_64-ffmpeg

    # build dependencies
    pacman -S mingw-w64-x86_64-make \
              mingw-w64-x86_64-gcc \
              mingw-w64-x86_64-pkg-config \
              mingw-w64-x86_64-meson \
              zip

Java (>= 7) is not available in MSYS2, so if you plan to build the server,
install it manually and make it available from the `PATH`:

    export PATH="$JAVA_HOME/bin:$PATH"


#### Mac OS

Use [Homebrew] to install the packages:

[Homebrew]: https://brew.sh/

    # runtime dependencies
    brew install sdl2 ffmpeg

    # build dependencies
    brew install gcc pkg-config meson zip

Java (>= 7) is not available in Homebrew, so if you plan to build the server,
install it manually and make it available from the `PATH`:

    export PATH="$JAVA_HOME/bin:$PATH"


### Common steps

Install the [Android SDK] (_Android Studio_), and set `ANDROID_HOME` to
its directory. For example:

[Android SDK]: https://developer.android.com/studio/index.html

    export ANDROID_HOME=~/android/sdk

Then, build `scrcpy`:

    meson x --buildtype release --strip -Db_lto=true
    cd x
    ninja

You can test it from here:

    ninja run

Or you can install it on the system:

    sudo ninja install    # without sudo on Windows

This installs two files:

 - `/usr/local/bin/scrcpy`
 - `/usr/local/share/scrcpy/scrcpy-server.jar`

Just remove them to "uninstall" the application.


#### Prebuilt server

Since the server binary, that will be pushed to the Android device, does not
depend on your system and architecture, you may want to use the prebuilt binary
instead: [`scrcpy-server.jar`](TODO).

In that case, the build does not require Java or the Android SDK.

Download the prebuilt server somewhere, and specify its path during the Meson
configuration:

    meson x --buildtype release --strip -Db_lto=true \
        -Dprebuilt_server=/path/to/scrcpy-server.jar
    cd x
    ninja
    sudo ninja install


## Run

_At runtime, `adb` must be accessible from your `PATH`._

If everything is ok, just plug an Android device, and execute:

    scrcpy

It accepts command-line arguments, listed by:

    scrcpy --help

For example, to decrease video bitrate to 2Mbps (default is 8Mbps):

    scrcpy -b 2M

To limit the video dimensions (e.g. if the device is 2540×1440, but the host
screen is smaller, or cannot decode such a high definition):

    scrcpy -m 1024

If several devices are listed in `adb devices`, you must specify the _serial_:

    scrcpy -s 0123456789abcdef


## Shortcuts

 | Action                                 |   Shortcut    |
 | -------------------------------------  | -------------:|
 | switch fullscreen mode                 | `Ctrl`+`f`    |
 | resize window to 1:1 (pixel-perfect)   | `Ctrl`+`g`    |
 | resize window to remove black borders  | `Ctrl`+`x`    |
 | click on `HOME`                        | `Ctrl`+`h`    |
 | click on `BACK`                        | `Ctrl`+`b`    |
 | click on `APP_SWITCH`                  | `Ctrl`+`m`    |
 | click on `VOLUME_UP`                   | `Ctrl`+`+`    |
 | click on `VOLUME_DOWN`                 | `Ctrl`+`-`    |
 | click on `POWER`                       | `Ctrl`+`p`    |
 | turn screen on                         | _Right-click_ |
 | enable/disable FPS counter (on stdout) | `Ctrl`+`i`    |


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
