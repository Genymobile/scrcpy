[f4c7044]: https://github.com/Genymobile/scrcpy/blob/f4c7044b46ae28eb64cb5e1a15c9649a44023c70/README.md

# scrcpy (v1.22)

<img src="app/data/icon.svg" width="128" height="128" alt="scrcpy" align="right" />

_发音为 "**scr**een **c**o**py**"_

本应用程序可以显示并控制通过 USB (或 [TCP/IP][article-tcpip]) 连接的安卓设备，且不需要任何 _root_ 权限。本程序支持 _GNU/Linux_, _Windows_ 和 _macOS_。

![screenshot](assets/screenshot-debian-600.jpg)

本应用专注于：

 - **轻量**： 原生，仅显示设备屏幕
 - **性能**： 30~120fps，取决于设备
 - **质量**： 分辨率可达 1920×1080 或更高
 - **低延迟**： [35~70ms][lowlatency]
 - **快速启动**： 最快 1 秒内即可显示第一帧
 - **对用户友好**： 无需帐号，无广告，无需联网，不会在设备上遗留任何程序
 - **免费**： 免费和开源软件

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646

功能：
 - [音频转发](doc/audio.md)（安卓11+）
 - [屏幕录制](#屏幕录制)
 - 镜像时[关闭设备屏幕](#关闭设备屏幕)
 - 双向[复制粘贴](#复制粘贴)
 - [可配置显示质量](#采集设置)
 - 以设备屏幕[作为摄像头(V4L2)](#v4l2loopback) (仅限 Linux)
 - [模拟物理键盘 (HID)](#物理键盘模拟-hid) (仅限 Linux)
 - [物理鼠标模拟 (HID)](#物理鼠标模拟-hid) (仅限 Linux)
 - [OTG模式](#otg) (仅限 Linux)
 - 更多 ……

## 系统要求

安卓设备最低需要支持 API 21 (Android 5.0)。

使用[音频转发]()，需要API>=30（Android 11+）。
在某些设备（尤其是小米）上，您可能会遇到以下错误：
```
java.lang.SecurityException: Injecting input events requires the caller (or the source of the instrumentation, if any) to have the INJECT_EVENTS permission.
```
在这种情况下，您需要启用一个额外的选项USB调试（安全设置）（这是一个与USB调试不同的项目），以便使用键盘和鼠标对其进行控制。设置此选项后，需要重新启动设备。

请注意，在OTG模式下运行scrcpy不需要USB调试。

## 获取本程序

 - [Linux](doc/linux.md)
 - [Windows](doc/windows.md)
 - [macOS](doc/macos.md)

## 示例

可以用在多个方面，关于使用方法，参见[文档]（#user-documentation）
这里只是一些常见的例子。

- 使用H.265（质量更好）捕获屏幕，将大小限制为1920，帧率限制为60fps，禁用音频，并通过模拟物理键盘来控制设备：
```bash
scrcpy --video-codec=h265 --max-size=1920 --max-fps=60 --no-audio --keyboard=uhid
scrcpy --video-codec=h265 -m1920 --max-fps=60 --no-audio -K  # 简写
```
- 使用设备摄像头以1920x1080的H.265格式（和麦克风）录制MP4文件
```bash
scrcpy --video-source=camera --video-codec=h265 --camera-size=1920x1080 --record=file.mp4
```
- 捕获设备前置摄像头，并将其作为网络摄像头展示在计算机上（在Linux上）：
```bash
scrcpy --video-source=camera --camera-size=1920x1080 --camera-facing=front --v4l2-sink=/dev/video2 --no-playback
```
- 通过模拟物理键盘和鼠标来控制设备（不使用镜像）（不需要USB调试）：
```bash
scrcpy --otg
```
- 使用插入计算机的游戏手柄控制器控制设备：
```bash
scrcpy --gamepad=uhid
scrcpy -G  #简写
```
## 用户文档
我们提供了许多功能和配置选项。它们记录在以下文档中：
- [连接](doc/connection.md)
- [视频](doc/video.md)
- [音频](doc/audio.md)
- [控制](doc/control.md)
- [键盘](doc/keyboard.md)
- [鼠标](doc/mouse.md)
- [游戏手柄](doc/gamepad.md)
- [设备](doc/device.cuankou)
- [窗口](doc/window.md)
- [录制](doc/recording.md)
- [tunnels](doc/tunnels.md)
- [OTG](doc/otg.md)
- [相机](doc/camera.md)
- [Video4Linux](doc/v4l2.md)
- [快捷方式](doc/shortcuts.md)

> 由于时间原因，我无法立刻完成翻译，用户文档大概会在2024/12/20前完成翻译
>
> Due to time constraints, I am unable to complete the translation immediately, and the userdocumentation will be completed by 2024/12/20

##资源
-[FAQ]（FAQ.md）
-[翻译][维基]（不一定是最新的）
-[编译说明]（doc/Build.md）
-[开发人员]（doc/develop.md）
[wiki]：https://github.com/Genymobile/scrcpy/wiki

> 由于时间原因，我无法立刻完成翻译，资源大概会在2024/12/20前完成翻译
>
> Due to time constraints, I am unable to complete the translation immediately, and the Resources will be completed by 2024/12/20

## 其它文档
-[介绍scrcpy][article-intro]
-[Srcpy现在可以无线工作][article-tcpip]
-[Srcpy 2.0，带音频][article-scrcpy2]
[article-intro]：https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]：https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
[article-scrcpy2]：https://blog.rom1v.com/2023/03/scrcpy-2-0-with-audio/

## 报告问题
通过[issue]您可以报告错误、功能请求或一般问题。
对于错误报告，请先阅读[FAQ]（FAQ.md），您可能会找到解决方案
立即解决您的问题。
[issue]：https://github.com/Genymobile/scrcpy/issues
您还可以通过以下方式联系我们：
-Reddit：[`r/scrcpy`](https://www.reddit.com/r/scrcpy)
-推特：[`@srcpy_app`](https://twitter.com/scrcpy_app)

## 捐赠
我是[@rom1v](https://github.com/rom1v)，scrcpy的作者和维护者。
如果你喜欢这个应用程序，你可以[支持我的开源工作][捐赠]：
-[GitHub](https://github.com/sponsors/rom1v)
-[Liberapay](https://liberapay.com/rom1v/)
-[PayPal](https://paypal.me/rom2v)
[捐赠]：https://blog.rom1v.com/about/#support-my-open-source-work

## 许可协议

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2024 Romain Vimont

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
