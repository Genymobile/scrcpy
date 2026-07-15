# Scrcpy Desk for macOS

`Scrcpy Desk` 是基于 scrcpy 仓库新增的 macOS SwiftUI 桌面端原型：

- 自动发现并展示 USB / Wi-Fi ADB 设备；
- 通过 IP 和端口执行 `adb connect`；
- 支持 Android 11+ 的 `adb pair` 无线配对；
- 选中设备后，直接在右侧创建低延迟 scrcpy 实时会话；
- 鼠标触控、滚动、键盘、快捷键、剪贴板和音频均由原 scrcpy 客户端处理；
- 不创建或唤起独立的 scrcpy 窗口。

## 环境

- macOS 13 或更高版本；
- Swift 5.10 或更高版本；
- ADB（构建脚本会将当前 PATH 中的 `adb` 复制进 App）；
- Homebrew 版 FFmpeg 与 SDL3 开发库（`brew install ffmpeg sdl3`）。

## 开发运行

```bash
./gradlew -p server assembleRelease
swift run ScrcpyDesk
```

## 构建 App

```bash
desktop/macos/ScrcpyDesk/build_app.sh
open "desktop/macos/ScrcpyDesk/dist/Scrcpy Desk.app"
```

也可传入自定义输出目录：

```bash
desktop/macos/ScrcpyDesk/build_app.sh /tmp/scrcpy-desk
```

## 连接设备

USB 设备只需启用开发者选项和 USB 调试，连接后会自动出现在左侧。

已有 ADB TCP/IP 地址时，点击“添加设备”，填写 IP 和连接端口（传统模式一般为 `5555`）。Android 11+ 首次使用无线调试时，打开“首次无线配对”，使用手机弹窗中显示的配对端口和六位配对码完成配对，再填写无线调试页显示的连接端口进行连接。

## 嵌入实现

App 与 scrcpy C 客户端被编译进同一个进程。右侧预览区域由绑定在 SwiftUI 主窗口上的无边框子渲染面承载，SDL3 只管理这块渲染面，不会创建独立应用窗口，也不会改变主界面布局。scrcpy 的服务端、视频解码器、音频播放器、输入管理器和控制通道保持不变；内部事件队列由 macOS 主事件循环以 60Hz 驱动，AppKit 的鼠标、滚轮、键盘和输入法事件则显式转发到 scrcpy。
