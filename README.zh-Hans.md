_Only the original [README](README.md) is guaranteed to be up-to-date._

只有原版的[README](README.md)会保持最新。

本文根据[ed130e05]进行翻译。

[ed130e05]: https://github.com/Genymobile/scrcpy/blob/ed130e05d55615d6014d93f15cfcb92ad62b01d8/README.md

# scrcpy (v1.17)

本应用程序可以显示并控制通过 USB (或 [TCP/IP][article-tcpip]) 连接的安卓设备，且不需要任何 _root_ 权限。本程序支持 _GNU/Linux_, _Windows_ 和 _macOS_。

![screenshot](assets/screenshot-debian-600.jpg)

它专注于：

 - **轻量** (原生，仅显示设备屏幕)
 - **性能** (30~60fps)
 - **质量** (分辨率可达 1920×1080 或更高)
 - **低延迟** ([35~70ms][lowlatency])
 - **快速启动** (最快 1 秒内即可显示第一帧)
 - **无侵入性** (不会在设备上遗留任何程序)

[lowlatency]: https://github.com/Genymobile/scrcpy/pull/646


## 系统要求

安卓设备最低需要支持 API 21 (Android 5.0)。

确保设备已[开启 adb 调试][enable-adb]。

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling

在某些设备上，还需要开启[额外的选项][control]以使用鼠标和键盘进行控制。

[control]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## 获取本程序

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Linux

在 Debian (目前仅支持 _testing_ 和 _sid_ 分支) 和Ubuntu (20.04) 上：

```
apt install scrcpy
```

我们也提供 [Snap] 包： [`scrcpy`][snap-link]。

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

对 Fedora 我们提供 [COPR] 包： [`scrcpy`][copr-link]。

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

对 Arch Linux 我们提供 [AUR] 包： [`scrcpy`][aur-link]。

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

对 Gentoo 我们提供 [Ebuild] 包：[`scrcpy/`][ebuild-link]。

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

您也可以[自行构建][BUILD] (不必担心，这并不困难)。



### Windows

在 Windows 上，简便起见，我们提供包含了所有依赖 (包括 `adb`) 的预编译包。

 - [README](README.md#windows)

也可以使用 [Chocolatey]：

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # 如果还没有 adb
```

或者 [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # 如果还没有 adb
```

[Scoop]: https://scoop.sh

您也可以[自行构建][BUILD]。


### macOS

本程序已发布到 [Homebrew]。直接安装即可：

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

你还需要在 `PATH` 内有 `adb`。如果还没有：

```bash
# Homebrew >= 2.6.0
brew install --cask android-platform-tools

# Homebrew < 2.6.0
brew cask install android-platform-tools
```

您也可以[自行构建][BUILD]。


## 运行

连接安卓设备，然后执行：

```bash
scrcpy
```

本程序支持命令行参数，查看参数列表：

```bash
scrcpy --help
```

## 功能介绍

### 捕获设置

#### 降低分辨率

有时候，可以通过降低镜像的分辨率来提高性能。

要同时限制宽度和高度到某个值 (例如 1024)：

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # 简写
```

另一边会被按比例缩小以保持设备的显示比例。这样，1920×1080 分辨率的设备会以 1024×576 的分辨率进行镜像。


#### 修改码率

默认码率是 8Mbps。要改变视频的码率 (例如改为 2Mbps)：

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # 简写
```

#### 限制帧率

要限制捕获的帧率：

```bash
scrcpy --max-fps 15
```

本功能从 Android 10 开始才被官方支持，但在一些旧版本中也能生效。

#### 画面裁剪

可以对设备屏幕进行裁剪，只镜像屏幕的一部分。

例如可以只镜像 Oculus Go 的一只眼睛。

```bash
scrcpy --crop 1224:1440:0:0   # 以 (0,0) 为原点的 1224x1440 像素
```

如果同时指定了 `--max-size`，会先进行裁剪，再进行缩放。


#### 锁定屏幕方向


要锁定镜像画面的方向：

```bash
scrcpy --lock-video-orientation 0   # 自然方向
scrcpy --lock-video-orientation 1   # 逆时针旋转 90°
scrcpy --lock-video-orientation 2   # 180°
scrcpy --lock-video-orientation 3   # 顺时针旋转 90°
```

只影响录制的方向。

[窗口可以独立旋转](#旋转)。


#### 编码器

一些设备内置了多种编码器，但是有的编码器会导致问题或崩溃。可以手动选择其它编码器：

```bash
scrcpy --encoder OMX.qcom.video.encoder.avc
```

要列出可用的编码器，可以指定一个不存在的编码器名称，错误信息中会包含所有的编码器：

```bash
scrcpy --encoder _
```

### 屏幕录制

可以在镜像的同时录制视频：

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

仅录制，不显示镜像：

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# 按 Ctrl+C 停止录制
```

录制时会包含“被跳过的帧”，即使它们由于性能原因没有实时显示。设备会为每一帧打上 _时间戳_ ，所以 [包时延抖动][packet delay variation] 不会影响录制的文件。

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


### 连接

#### 无线

_Scrcpy_ 使用 `adb` 与设备通信，并且 `adb` 支持通过 TCP/IP [连接]到设备:

1. 将设备和电脑连接至同一 Wi-Fi。
2. 打开 设置 → 关于手机 → 状态信息，获取设备的 IP 地址，也可以执行以下的命令：
    ```bash
    adb shell ip route | awk '{print $9}'
    ```

3. 启用设备的网络 adb 功能 `adb tcpip 5555`。
4. 断开设备的 USB 连接。
5. 连接到您的设备：`adb connect DEVICE_IP:5555` _(将 `DEVICE_IP` 替换为设备 IP)_.
6. 正常运行 `scrcpy`。

可能需要降低码率和分辨率：

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # 简写
```

[连接]: https://developer.android.com/studio/command-line/adb.html#wireless


#### 多设备

如果 `adb devices` 列出了多个设备，您必须指定设备的 _序列号_ ：

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # 简写
```

如果设备通过 TCP/IP 连接：

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # 简写
```

您可以同时启动多个 _scrcpy_ 实例以同时显示多个设备的画面。

#### 在设备连接时自动启动

您可以使用 [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### SSH 隧道

要远程连接到设备，可以将本地的 adb 客户端连接到远程的 adb 服务端 (需要两端的 _adb_ 协议版本相同)：

```bash
adb kill-server    # 关闭本地 5037 端口上的 adb 服务端
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# 保持该窗口开启
```

在另一个终端：

```bash
scrcpy
```

若要不使用远程端口转发，可以强制使用正向连接 (注意 `-L` 和 `-R` 的区别)：

```bash
adb kill-server    # 关闭本地 5037 端口上的 adb 服务端
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# 保持该窗口开启
```

在另一个终端:

```bash
scrcpy --force-adb-forward
```


类似无线网络连接，可能需要降低画面质量：

```
scrcpy -b2M -m800 --max-fps 15
```

### 窗口设置

#### 标题

窗口的标题默认为设备型号。可以通过如下命令修改：

```bash
scrcpy --window-title 'My device'
```

#### 位置和大小

您可以指定初始的窗口位置和大小：

```bash
scrcpy --window-x 100 --window-y 100 --window-width 800 --window-height 600
```

#### 无边框

关闭边框：

```bash
scrcpy --window-borderless
```

#### 保持窗口在最前

您可以通过如下命令保持窗口在最前面：

```bash
scrcpy --always-on-top
```

#### 全屏

您可以通过如下命令直接全屏启动scrcpy：

```bash
scrcpy --fullscreen
scrcpy -f  # 简写
```

全屏状态可以通过 <kbd>MOD</kbd>+<kbd>f</kbd> 随时切换。

#### 旋转

可以通过以下命令旋转窗口：

```bash
scrcpy --rotation 1
```

可选的值有：
 - `0`: 无旋转
 - `1`: 逆时针旋转 90°
 - `2`: 旋转 180°
 - `3`: 顺时针旋转 90°

也可以使用 <kbd>MOD</kbd>+<kbd>←</kbd> _(左箭头)_ 和 <kbd>MOD</kbd>+<kbd>→</kbd> _(右箭头)_ 随时更改。

需要注意的是， _scrcpy_ 有三个不同的方向：
 - <kbd>MOD</kbd>+<kbd>r</kbd> 请求设备在竖屏和横屏之间切换 (如果前台应用程序不支持请求的朝向，可能会拒绝该请求)。
 - [`--lock-video-orientation`](#锁定屏幕方向) 改变镜像的朝向 (设备传输到电脑的画面的朝向)。这会影响录制。
 - `--rotation` (或 <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>) 只旋转窗口的内容。这只影响显示，不影响录制。


### 其他镜像设置

#### 只读

禁用电脑对设备的控制 (如键盘输入、鼠标事件和文件拖放)：

```bash
scrcpy --no-control
scrcpy -n
```

#### 显示屏

如果设备有多个显示屏，可以选择要镜像的显示屏：

```bash
scrcpy --display 1
```

可以通过如下命令列出所有显示屏的 id：

```
adb shell dumpsys display   # 在输出中搜索 “mDisplayId=”
```

控制第二显示屏需要设备运行 Android 10 或更高版本 (否则将在只读状态下镜像)。


#### 保持常亮

阻止设备在连接时休眠：

```bash
scrcpy --stay-awake
scrcpy -w
```

程序关闭时会恢复设备原来的设置。


#### 关闭设备屏幕

可以通过以下的命令行参数在关闭设备屏幕的状态下进行镜像：

```bash
scrcpy --turn-screen-off
scrcpy -S
```

或者在任何时候按 <kbd>MOD</kbd>+<kbd>o</kbd>。

要重新打开屏幕，按下 <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

在Android上，`电源` 按钮始终能把屏幕打开。为了方便，对于在 _scrcpy_ 中发出的 `电源` 事件 (通过鼠标右键或 <kbd>MOD</kbd>+<kbd>p</kbd>)，会 (尽最大的努力) 在短暂的延迟后将屏幕关闭。设备上的 `电源` 按钮仍然能打开设备屏幕。

还可以同时阻止设备休眠：

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```


#### 渲染过期帧

默认状态下，为了降低延迟， _scrcpy_ 永远渲染解码成功的最近一帧，并跳过前面任意帧。

强制渲染所有帧 (可能导致延迟变高)：

```bash
scrcpy --render-expired-frames
```

#### 显示触摸

在演示时，可能会需要显示物理触摸点 (在物理设备上的触摸点)。

Android 在 _开发者选项_ 中提供了这项功能。

_Scrcpy_ 提供一个选项可以在启动时开启这项功能并在退出时恢复初始设置：

```bash
scrcpy --show-touches
scrcpy -t
```

请注意这项功能只能显示 _物理_ 触摸 (用手指在屏幕上的触摸)。


#### 关闭屏保

_Scrcpy_ 默认不会阻止电脑上开启的屏幕保护。

关闭屏幕保护：

```bash
scrcpy --disable-screensaver
```


### 输入控制

#### 旋转设备屏幕

使用 <kbd>MOD</kbd>+<kbd>r</kbd> 在竖屏和横屏模式之间切换。

需要注意的是，只有在前台应用程序支持所要求的模式时，才会进行切换。

#### 复制粘贴

每次安卓的剪贴板变化时，其内容都会被自动同步到电脑的剪贴板上。

所有的 <kbd>Ctrl</kbd> 快捷键都会被转发至设备。其中：
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> 通常执行复制
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> 通常执行剪切
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> 通常执行粘贴 (在电脑到设备的剪贴板同步完成之后)

大多数时候这些按键都会执行以上的功能。

但实际的行为取决于设备上的前台程序。例如，_Termux_ 会在按下 <kbd>Ctrl</kbd>+<kbd>c</kbd> 时发送 SIGINT，又如 _K-9 Mail_ 会新建一封邮件。

要在这种情况下进行剪切，复制和粘贴 (仅支持 Android >= 7)：
 - <kbd>MOD</kbd>+<kbd>c</kbd> 注入 `COPY` (复制)
 - <kbd>MOD</kbd>+<kbd>x</kbd> 注入 `CUT` (剪切)
 - <kbd>MOD</kbd>+<kbd>v</kbd> 注入 `PASTE` (粘贴) (在电脑到设备的剪贴板同步完成之后)

另外，<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> 会将电脑的剪贴板内容转换为一串按键事件输入到设备。在应用程序不接受粘贴时 (比如 _Termux_)，这项功能可以派上一定的用场。不过这项功能可能会导致非 ASCII 编码的内容出现错误。

**警告：** 将电脑剪贴板的内容粘贴至设备 (无论是通过 <kbd>Ctrl</kbd>+<kbd>v</kbd> 还是 <kbd>MOD</kbd>+<kbd>v</kbd>) 都会将内容复制到设备的剪贴板。如此，任何安卓应用程序都能读取到。您应避免将敏感内容 (如密码) 通过这种方式粘贴。

一些设备不支持通过程序设置剪贴板。通过 `--legacy-paste` 选项可以修改 <kbd>Ctrl</kbd>+<kbd>v</kbd> 和 <kbd>MOD</kbd>+<kbd>v</kbd> 的工作方式，使它们通过按键事件 (同 <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>) 来注入电脑剪贴板内容。

#### 双指缩放

模拟“双指缩放”：<kbd>Ctrl</kbd>+_按住并移动鼠标_。

更准确的说，在按住鼠标左键时按住 <kbd>Ctrl</kbd>。直到松开鼠标左键，所有鼠标移动将以屏幕中心为原点，缩放或旋转内容 (如果应用支持)。

实际上，_scrcpy_ 会在以屏幕中心对称的位置上生成由“虚拟手指”发出的额外触摸事件。


#### 文字注入偏好

打字的时候，系统会产生两种[事件][textevents]：
 - _按键事件_ ，代表一个按键被按下或松开。
 - _文本事件_ ，代表一个字符被输入。

程序默认使用按键事件来输入字母。只有这样，键盘才会在游戏中正常运作 (例如 WASD 键)。

但这也有可能[造成一些问题][prefertext]。如果您遇到了问题，可以通过以下方式避免：

```bash
scrcpy --prefer-text
```

(这会导致键盘在游戏中工作不正常)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### 按键重复

默认状态下，按住一个按键不放会生成多个重复按键事件。在某些游戏中这可能会导致性能问题。

避免转发重复按键事件：

```bash
scrcpy --no-key-repeat
```


#### 右键和中键

默认状态下，右键会触发返回键 (或电源键)，中键会触发 HOME 键。要禁用这些快捷键并把所有点击转发到设备：

```bash
scrcpy --forward-all-clicks
```


### 文件拖放

#### 安装APK

将 APK 文件 (文件名以 `.apk` 结尾) 拖放到 _scrcpy_ 窗口来安装。

该操作在屏幕上不会出现任何变化，而会在控制台输出一条日志。


#### 将文件推送至设备

要推送文件到设备的 `/sdcard/`，将 (非 APK) 文件拖放至 _scrcpy_ 窗口。

该操作没有可见的响应，只会在控制台输出日志。

在启动时可以修改目标目录：

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### 音频转发

_Scrcpy_ 不支持音频。请使用 [sndcpy].

另外请阅读 [issue #14]。

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## 快捷键

在以下列表中, <kbd>MOD</kbd> 是快捷键的修饰键。
默认是 (左) <kbd>Alt</kbd> 或 (左) <kbd>Super</kbd>。

您可以使用 `--shortcut-mod` 来修改。可选的按键有 `lctrl`、`rctrl`、`lalt`、`ralt`、`lsuper` 和 `rsuper`。例如：

```bash
# 使用右 Ctrl 键
scrcpy --shortcut-mod=rctrl

# 使用左 Ctrl 键 + 左 Alt 键，或 Super 键
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> 键通常是指 <kbd>Windows</kbd> 或 <kbd>Cmd</kbd> 键。_

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | 操作                              | 快捷键                                       |
 | --------------------------------- | :------------------------------------------- |
 | 全屏                              | <kbd>MOD</kbd>+<kbd>f</kbd>                  |
 | 向左旋转屏幕                      | <kbd>MOD</kbd>+<kbd>←</kbd> _(左箭头)_       |
 | 向右旋转屏幕                      | <kbd>MOD</kbd>+<kbd>→</kbd> _(右箭头)_       |
 | 将窗口大小重置为1:1 (匹配像素)    | <kbd>MOD</kbd>+<kbd>g</kbd>                  |
 | 将窗口大小重置为消除黑边          | <kbd>MOD</kbd>+<kbd>w</kbd> \| _双击¹_       |
 | 点按 `主屏幕`                     | <kbd>MOD</kbd>+<kbd>h</kbd> \| _鼠标中键_    |
 | 点按 `返回`                       | <kbd>MOD</kbd>+<kbd>b</kbd> \| _鼠标右键²_   |
 | 点按 `切换应用`                   | <kbd>MOD</kbd>+<kbd>s</kbd>                  |
 | 点按 `菜单` (解锁屏幕)            | <kbd>MOD</kbd>+<kbd>m</kbd>                  |
 | 点按 `音量+`                      | <kbd>MOD</kbd>+<kbd>↑</kbd> _(上箭头)_       |
 | 点按 `音量-`                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(下箭头)_       |
 | 点按 `电源`                       | <kbd>MOD</kbd>+<kbd>p</kbd>                  |
 | 打开屏幕                          | _鼠标右键²_                                  |
 | 关闭设备屏幕 (但继续在电脑上显示) | <kbd>MOD</kbd>+<kbd>o</kbd>                  |
 | 打开设备屏幕                      | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd> |
 | 旋转设备屏幕                      | <kbd>MOD</kbd>+<kbd>r</kbd>                  |
 | 展开通知面板                      | <kbd>MOD</kbd>+<kbd>n</kbd>                  |
 | 收起通知面板                      | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd> |
 | 复制到剪贴板³                     | <kbd>MOD</kbd>+<kbd>c</kbd>                  |
 | 剪切到剪贴板³                     | <kbd>MOD</kbd>+<kbd>x</kbd>                  |
 | 同步剪贴板并粘贴³                 | <kbd>MOD</kbd>+<kbd>v</kbd>                  |
 | 注入电脑剪贴板文本                | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> |
 | 打开/关闭FPS显示 (在 stdout)      | <kbd>MOD</kbd>+<kbd>i</kbd>                  |
 | 捏拉缩放                          | <kbd>Ctrl</kbd>+_按住并移动鼠标_             |

_¹双击黑边可以去除黑边_
_²点击鼠标右键将在屏幕熄灭时点亮屏幕，其余情况则视为按下返回键 。_
_³需要安卓版本 Android >= 7。_

所有的 <kbd>Ctrl</kbd>+_按键_ 的快捷键都会被转发到设备，所以会由当前应用程序进行处理。


## 自定义路径

要使用指定的 _adb_ 二进制文件，可以设置环境变量 `ADB`：

    ADB=/path/to/adb scrcpy

要覆盖 `scrcpy-server` 的路径，可以设置 `SCRCPY_SERVER_PATH`。

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## 为什么叫 _scrcpy_ ？

一个同事让我找出一个和 [gnirehtet] 一样难以发音的名字。

[`strcpy`] 复制一个 **str**ing； `scrcpy` 复制一个 **scr**een。

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## 如何构建？

请查看[BUILD]。

[BUILD]: BUILD.md


## 常见问题

请查看[FAQ](FAQ.md)。


## 开发者

请查看[开发者页面]。

[开发者页面]: DEVELOP.md


## 许可协议

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2021 Romain Vimont

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

## 相关文章

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
