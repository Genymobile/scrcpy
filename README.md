> [!NOTE]
> **Slink is a rebranded fork based on the upstream [Genymobile/scrcpy](https://github.com/Genymobile/scrcpy) project.**
> The upstream project and its authors retain their original copyrights and attribution.

# Slink (v4.1)

<img src="app/data/scrcpy.svg" width="128" height="128" alt="Slink" align="right" />

**Slink** mirrors Android devices (video and audio) connected via USB or TCP/IP and allows control using the computer's keyboard and mouse. It does not require root access or an app installed on the device. It works on Linux, Windows, and macOS.

[![Linux](https://img.shields.io/badge/Linux-download-orange?style=for-the-badge&logo=linux)](doc/linux.md)&nbsp;
[![Windows](https://img.shields.io/badge/Windows-download-blue?style=for-the-badge&logo=windows)](doc/windows.md)&nbsp;
[![macOS](https://img.shields.io/badge/macOS-download-brightgreen?style=for-the-badge&logo=apple)](doc/macos.md)&nbsp;

## Highlights

- **Lightweight** native application
- **High performance** up to 120 fps depending on the device
- **High quality** 1920×1080 or above
- **Low latency**
- **Fast startup**
- **Non-intrusive** — nothing is left installed on the Android device
- **No account, ads, or internet requirement**
- **Free and open source**

## Features

- Audio forwarding (Android 11+)
- Screen and camera recording
- Virtual displays
- Mirroring with the Android screen off
- Bidirectional copy/paste
- Configurable video quality
- Camera mirroring (Android 12+)
- V4L2 webcam support on Linux
- Physical keyboard and mouse simulation (HID)
- Gamepad support
- OTG mode

## Prerequisites

The Android device requires at least API 21 (Android 5.0).

Make sure USB debugging is enabled on the device. See the [Android USB debugging documentation](https://developer.android.com/studio/debug/dev-options#enable).

USB debugging is not required for Slink OTG mode.

## Get Slink

- [Linux](doc/linux.md)
- [Windows](doc/windows.md)
- [macOS](doc/macos.md)

## Usage

The command-line executable is **`slink`** (Windows: **`slink.exe`**).

```bash
slink
slink --serial ABC123
slink --select-usb
slink --select-tcpip
slink --fullscreen
slink --otg
```

### Common examples

Limit the mirrored video to 1920 pixels, 60 fps, and disable audio:

```bash
slink --video-codec=h265 --max-size=1920 --max-fps=60 --no-audio
```

Start VLC in a new virtual display:

```bash
slink --new-display=1920x1080 --start-app=org.videolan.vlc
```

Record the device camera to an MP4 file:

```bash
slink --video-source=camera --video-codec=h265 --camera-size=1920x1080 --record=file.mp4
```

Control the device without mirroring:

```bash
slink --otg
```

## Documentation

- [Connection](doc/connection.md)
- [Video](doc/video.md)
- [Audio](doc/audio.md)
- [Control](doc/control.md)
- [Keyboard](doc/keyboard.md)
- [Mouse](doc/mouse.md)
- [Gamepad](doc/gamepad.md)
- [Device](doc/device.md)
- [Window](doc/window.md)
- [Recording](doc/recording.md)
- [Virtual display](doc/virtual-display.md)
- [Tunnels](doc/tunnels.md)
- [OTG](doc/otg.md)
- [Camera](doc/camera.md)
- [Video4Linux](doc/v4l2.md)
- [Shortcuts](doc/shortcuts.md)
- [Build instructions](doc/build.md)
- [Developer documentation](doc/develop.md)

## Upstream project

Slink is based on the excellent [Genymobile/scrcpy](https://github.com/Genymobile/scrcpy) project. For upstream documentation, historical articles, and original project discussions, please refer to the upstream repository.

## License and attribution

Slink remains distributed under the Apache License 2.0. Original upstream copyrights, notices, and attribution are retained in this repository. The Slink rebranding does not claim authorship of the original upstream work.
