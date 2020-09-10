_Only the original [README](README.md) is guaranteed to be up-to-date._

只有原版的[README](README.md)会保持最新。

本文根据[479d10d]进行翻译。

[479d10d]: https://github.com/Genymobile/scrcpy/commit/479d10dc22b70272187e0963c6ad24d754a669a2#diff-04c6e90faac2675aa89e2176d2eec7d8



# scrcpy (v1.16)

本应用程序可以通过USB（或 [TCP/IP][article-tcpip] ）连接用于显示或控制安卓设备。这不需要获取 _root_ 权限。

该应用程序可以在 _GNU/Linux_, _Windows_ 和 _macOS_ 环境下运行。

[article-tcpip]:https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/

![screenshot](assets/screenshot-debian-600.jpg)

它专注于：

 - **轻量** （原生，仅显示设备屏幕）
 - **性能** （30~60fps）
 - **质量** （分辨率可达1920x1080或更高）
 - **低延迟** (35-70ms)
 - **快速启动** （数秒内即能开始显示）
 - **无侵入性** （不需要在安卓设备上安装任何程序）


## 使用要求

安卓设备系统版本需要在Android 5.0（API 21)或以上。

确保您在设备上开启了[adb调试]。

[adb调试]: https://developer.android.com/studio/command-line/adb.html#Enabling

在某些设备上，你还需要开启[额外的选项]以用鼠标和键盘进行控制。

[额外的选项]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


## 获取scrcpy

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Linux

在Debian（目前仅测试版和不稳定版，即 _testing_ 和 _sid_ 版本）和Ubuntu （20.04）上：

```
apt install scrcpy
```

[Snap]包也是可用的： [`scrcpy`][snap-link].

[snap-link]: https://snapstats.org/snaps/scrcpy

[snap]: https://en.wikipedia.org/wiki/Snappy_(package_manager)

对于Fedora用户，我们提供[COPR]包： [`scrcpy`][copr-link].

[COPR]: https://fedoraproject.org/wiki/Category:Copr
[copr-link]: https://copr.fedorainfracloud.org/coprs/zeno/scrcpy/

对于Arch Linux用户，我们提供[AUR]包： [`scrcpy`][aur-link].

[AUR]: https://wiki.archlinux.org/index.php/Arch_User_Repository
[aur-link]: https://aur.archlinux.org/packages/scrcpy/

对于Gentoo用户，我们提供[Ebuild]包：[`scrcpy/`][ebuild-link].

[Ebuild]: https://wiki.gentoo.org/wiki/Ebuild
[ebuild-link]: https://github.com/maggu2810/maggu2810-overlay/tree/master/app-mobilephone/scrcpy

您也可以[自行编译][编译](不必担心，这并不困难)。



### Windows

在Windows上，简便起见，我们准备了包含所有依赖项（包括adb）的程序包。

 - [`scrcpy-win64-v1.16.zip`][direct-win64]  
   _(SHA-256: 3f30dc5db1a2f95c2b40a0f5de91ec1642d9f53799250a8c529bc882bc0918f0)_

[direct-win64]: https://github.com/Genymobile/scrcpy/releases/download/v1.16/scrcpy-win64-v1.16.zip

您也可以在[Chocolatey]下载：

[Chocolatey]: https://chocolatey.org/

```bash
choco install scrcpy
choco install adb    # 如果你没有adb
```

也可以使用 [Scoop]:

```bash
scoop install scrcpy
scoop install adb    # 如果你没有adb
```

[Scoop]: https://scoop.sh

您也可以[自行编译][编译]。


### macOS

您可以使用[Homebrew]下载scrcpy。直接安装就可以了：

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

您需要 `adb`以使用scrcpy，并且它需要可以通过 `PATH`被访问。如果您没有：

```bash
brew cask install android-platform-tools
```

您也可以[自行编译][编译]。


## 运行scrcpy

用USB链接电脑和安卓设备，并执行：

```bash
scrcpy
```

支持带命令行参数执行，查看参数列表：

```bash
scrcpy --help
```

## 功能介绍

### 画面设置

#### 缩小分辨率

有时候，将设备屏幕镜像分辨率降低可以有效地提升性能。

我们可以将高度和宽度都限制在一定大小内（如 1024）：

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # short version
```

较短的一边会被按比例缩小以保持设备的显示比例。
这样，1920x1080 的设备会以 1024x576 的分辨率显示。


#### 修改画面比特率

默认的比特率是8Mbps。如果要改变画面的比特率 (比如说改成2Mbps)：

```bash
scrcpy --bit-rate 2M
scrcpy -b 2M  # short version
```

#### 限制画面帧率

画面的帧率可以通过下面的命令被限制：

```bash
scrcpy --max-fps 15
```

这个功能仅在Android 10和以后的版本被Android官方支持，但也有可能在更早的版本可用。

#### 画面裁剪

设备画面可在裁切后进行镜像，以显示部分屏幕。

这项功能可以用于，例如，只显示Oculus Go的一只眼睛。

```bash
scrcpy --crop 1224:1440:0:0   # 1224x1440 at offset (0,0)
```

如果`--max-size`在同时被指定，分辨率的改变将在画面裁切后进行。


#### 锁定屏幕朝向


可以使用如下命令锁定屏幕朝向：

```bash
scrcpy --lock-video-orientation 0   # 自然朝向
scrcpy --lock-video-orientation 1   # 90° 逆时针旋转
scrcpy --lock-video-orientation 2   # 180°
scrcpy --lock-video-orientation 3   # 90° 顺时针旋转
```

该设定影响录制。


### 屏幕录制

可以在屏幕镜像的同时录制视频：

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

在不开启屏幕镜像的同时录制：

```bash
scrcpy --no-display --record file.mp4
scrcpy -Nr file.mkv
# 按Ctrl+C以停止录制
```

在显示中“被跳过的帧”会被录制，虽然它们由于性能原因没有实时显示。
在传输中每一帧都有 _时间戳_ ，所以 [包时延变化] 并不影响录制的文件。

[包时延变化]: https://en.wikipedia.org/wiki/Packet_delay_variation


### 连接方式

#### 无线

_Scrcpy_ 使用`adb`来与安卓设备连接。同时，`adb`能够通过TCP/IP[连接]到安卓设备:

1. 将您的安卓设备和电脑连接至同一Wi-Fi。
2. 获取安卓设备的IP地址（在设置-关于手机-状态信息）。
3. 打开安卓设备的网络adb功能`adb tcpip 5555`。
4. 将您的设备与电脑断开连接。
5. 连接到您的设备：`adb connect DEVICE_IP:5555` _(用设备IP替换 `DEVICE_IP`)_.
6. 运行`scrcpy`。

降低比特率和分辨率可能有助于性能：

```bash
scrcpy --bit-rate 2M --max-size 800
scrcpy -b2M -m800  # short version
```

[连接]: https://developer.android.com/studio/command-line/adb.html#wireless


#### 多设备

如果多个设备在执行`adb devices`后被列出，您必须指定设备的 _序列号_ ：

```bash
scrcpy --serial 0123456789abcdef
scrcpy -s 0123456789abcdef  # short version
```

如果设备是通过TCP/IP方式连接到电脑的：

```bash
scrcpy --serial 192.168.0.1:5555
scrcpy -s 192.168.0.1:5555  # short version
```

您可以同时启动多个 _scrcpy_ 实例以同时显示多个设备的画面。

#### 在设备连接时自动启动

您可以使用 [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### SSH 连接

本地的 adb 可以远程连接到另一个 adb 服务器（假设两者的adb版本相同），来远程连接到设备：

```bash
adb kill-server    # 关闭本地5037端口上的adb服务器
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# 保持该窗口开启
```

从另一个终端：

```bash
scrcpy
```

为了避免启动远程端口转发，你可以强制启动一个转发连接（注意`-L`和`-R`的区别：

```bash
adb kill-server    # kill the local adb server on 5037
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# 保持该窗口开启
```

从另一个终端:

```bash
scrcpy --force-adb-forward
```


和无线网络连接类似，下列设置可能对改善性能有帮助：

```
scrcpy -b2M -m800 --max-fps 15
```

### 窗口设置

#### 标题

窗口的标题默认为设备型号。您可以通过如下命令修改它：

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
scrcpy -f  # short version
```

全屏状态可以通过<kbd>MOD</kbd>+<kbd>f</kbd>实时改变。

#### 旋转

通过如下命令，窗口可以旋转：

```bash
scrcpy --rotation 1
```

可选的值有：
 - `0`: 无旋转
 - `1`: 逆时针旋转90°
 - `2`: 旋转180°
 - `3`: 顺时针旋转90°

这同样可以使用<kbd>MOD</kbd>+<kbd>←</kbd>
_(左)_ 和 <kbd>MOD</kbd>+<kbd>→</kbd> _(右)_ 的快捷键实时更改。

需要注意的是， _scrcpy_ 控制三个不同的朝向：
 - <kbd>MOD</kbd>+<kbd>r</kbd> 请求设备在竖屏和横屏之间切换（如果前台应用程序不支持所请求的朝向，可能会拒绝该请求）。

 - `--lock-video-orientation` 改变镜像的朝向（设备镜像到电脑的画面朝向）。这会影响录制。

 - `--rotation` （或<kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>）
   只旋转窗口的画面。这只影响显示，不影响录制。


### 其他镜像设置

#### 只读

关闭电脑对设备的控制（如键盘输入、鼠标移动和文件传输）：

```bash
scrcpy --no-control
scrcpy -n
```

#### 显示屏

如果有多个显示屏可用，您可以选择特定显示屏进行镜像：

```bash
scrcpy --display 1
```

您可以通过如下命令找到显示屏的id：

```
adb shell dumpsys display   # 在回显中搜索“mDisplayId=”
```

第二显示屏可能只能在设备运行Android 10或以上的情况下被控制（它可能会在电脑上显示，但无法通过电脑操作）。


#### 保持常亮

防止设备在已连接的状态下休眠：

```bash
scrcpy --stay-awake
scrcpy -w
```

程序关闭后，设备设置会恢复原样。


#### 关闭设备屏幕

在启动屏幕镜像时，可以通过如下命令关闭设备的屏幕：

```bash
scrcpy --turn-screen-off
scrcpy -S
```

或者在需要的时候按<kbd>MOD</kbd>+<kbd>o</kbd>。

要重新打开屏幕的话，需要按<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

在Android上,`电源`按钮始终能把屏幕打开。

为了方便，如果按下`电源`按钮的事件是通过 _scrcpy_ 发出的（通过点按鼠标右键或<kbd>MOD</kbd>+<kbd>p</kbd>），它会在短暂的延迟后将屏幕关闭。

物理的`电源`按钮仍然能打开设备屏幕。

同时，这项功能还能被用于防止设备休眠：

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```


#### 渲染超时帧

为了降低延迟， _scrcpy_ 默认渲染解码成功的最近一帧，并跳过前面任意帧。

强制渲染所有帧（可能导致延迟变高）：

```bash
scrcpy --render-expired-frames
```

#### 显示触摸

在展示时，有些时候可能会用到显示触摸点这项功能（在设备上显示）。

Android在 _开发者设置_ 中提供了这项功能。

_Scrcpy_ 提供一个选项可以在启动时开启这项功能并在退出时恢复初始设置：

```bash
scrcpy --show-touches
scrcpy -t
```

请注意这项功能只能显示 _物理_ 触摸（要用手在屏幕上触摸）。


#### 关闭屏保

_Scrcpy_ 不会默认关闭屏幕保护。

关闭屏幕保护：

```bash
scrcpy --disable-screensaver
```


### 输入控制

#### 旋转设备屏幕

使用<kbd>MOD</kbd>+<kbd>r</kbd>以在竖屏和横屏模式之间切换。

需要注意的是，只有在前台应用程序支持所要求的模式时，才会进行切换。

#### 复制黏贴

每次Android的剪贴板变化的时候，它都会被自动同步到电脑的剪贴板上。

所有的 <kbd>Ctrl</kbd> 快捷键都会被转发至设备。其中：
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> 复制
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> 剪切
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> 黏贴 （在电脑到设备的剪贴板同步完成之后）

这通常如您所期望的那样运作。

但实际的行为取决于设备上的前台程序。
例如 _Termux_ 在<kbd>Ctrl</kbd>+<kbd>c</kbd>被按下时发送 SIGINT，
又如 _K-9 Mail_ 会新建一封新邮件。

在这种情况下剪切复制黏贴（仅在Android >= 7时可用）：
 - <kbd>MOD</kbd>+<kbd>c</kbd> 注入 `COPY`（复制）
 - <kbd>MOD</kbd>+<kbd>x</kbd> 注入 `CUT`（剪切）
 - <kbd>MOD</kbd>+<kbd>v</kbd> 注入 `PASTE`（黏贴）（在电脑到设备的剪贴板同步完成之后）

另外，<kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>可以将电脑的剪贴板内容转换为一串按键事件输入到设备。
在应用程序不接受黏贴时（比如 _Termux_ ），这项功能可以排上一定的用场。
需要注意的是，这项功能可能会导致非ASCII编码的内容出现错误。

**警告：** 将电脑剪贴板的内容黏贴至设备（无论是通过<kbd>Ctrl</kbd>+<kbd>v</kbd>还是<kbd>MOD</kbd>+<kbd>v</kbd>）
都需要将内容保存至设备的剪贴板。如此，任何一个应用程序都可以读取它。
您应当避免将敏感内容通过这种方式传输（如密码）。


#### 捏拉缩放

模拟 “捏拉缩放”：<kbd>Ctrl</kbd>+_按住并移动鼠标_。

更准确的说，您需要在按住<kbd>Ctrl</kbd>的同时按住并移动鼠标。
在鼠标左键松开之后，光标的任何操作都会相对于屏幕的中央进行。

具体来说， _scrcpy_ 使用“虚拟手指”以在相对于屏幕中央相反的位置产生触摸事件。


#### 文字注入偏好

打字的时候，系统会产生两种[事件][textevents]：
 - _按键事件_ ，代表一个按键被按下/松开。
 - _文本事件_ ，代表一个文本被输入。

程序默认使用按键事件来输入字母。只有这样，键盘才会在游戏中正常运作（尤其WASD键）。

但这也有可能[造成问题][prefertext]。如果您遇到了这样的问题，您可以通过下列操作避免它：

```bash
scrcpy --prefer-text
```

（这会导致键盘在游戏中工作不正常）

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### 按键重复

当你一直按着一个按键不放时，程序默认产生多个按键事件。
在某些游戏中这可能会导致性能问题。

避免转发重复按键事件：

```bash
scrcpy --no-key-repeat
```


### 文件传输

#### 安装APK

如果您要要安装APK，请拖放APK文件（文件名以`.apk`结尾）到 _scrcpy_ 窗口。

该操作在屏幕上不会出现任何变化，而会在控制台输出一条日志。


#### 将文件推送至设备

如果您要推送文件到设备的 `/sdcard/`，请拖放文件至（不能是APK文件）_scrcpy_ 窗口。

该操作没有可见的响应，只会在控制台输出日志。

在启动时可以修改目标目录：

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### 音频转发

_scrcpy_ 不支持音频。请使用 [sndcpy].

另外请阅读 [issue #14]。

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## 热键

在下列表格中, <kbd>MOD</kbd> 是热键的修饰键。
默认是（左）<kbd>Alt</kbd>或者（左）<kbd>Super</kbd>。

您可以使用 `--shortcut-mod`后缀来修改它。可选的按键有`lctrl`、`rctrl`、
`lalt`、`ralt`、`lsuper`和`rsuper`。如下例：

```bash
# 使用右侧的Ctrl键
scrcpy --shortcut-mod=rctrl

# 使用左侧的Ctrl键、Alt键或Super键
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_一般来说，<kbd>[Super]</kbd>就是<kbd>Windows</kbd>或者<kbd>Cmd</kbd>。_

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | 操作                                        |   快捷键
 | ------------------------------------------- |:-----------------------------
 | 全屏                                        | <kbd>MOD</kbd>+<kbd>f</kbd>
 | 向左旋转屏幕                                 | <kbd>MOD</kbd>+<kbd>←</kbd> _(左)_
 | 向右旋转屏幕                                 | <kbd>MOD</kbd>+<kbd>→</kbd> _(右)_
 | 将窗口大小重置为1:1 (像素优先)                | <kbd>MOD</kbd>+<kbd>g</kbd>
 | 将窗口大小重置为消除黑边                      | <kbd>MOD</kbd>+<kbd>w</kbd> \| _双击¹_
 | 点按 `主屏幕`                                | <kbd>MOD</kbd>+<kbd>h</kbd> \| _点击鼠标中键_
 | 点按 `返回`                                  | <kbd>MOD</kbd>+<kbd>b</kbd> \| _点击鼠标右键²_
 | 点按 `切换应用`                              | <kbd>MOD</kbd>+<kbd>s</kbd>
 | 点按 `菜单` （解锁屏幕）                      | <kbd>MOD</kbd>+<kbd>m</kbd>
 | 点按 `音量+`                                | <kbd>MOD</kbd>+<kbd>↑</kbd> _(up)_
 | 点按 `音量-`                                | <kbd>MOD</kbd>+<kbd>↓</kbd> _(down)_
 | 点按 `电源`                                 | <kbd>MOD</kbd>+<kbd>p</kbd>
 | 打开屏幕                                    | _点击鼠标右键²_
 | 关闭设备屏幕（但继续在电脑上显示）             | <kbd>MOD</kbd>+<kbd>o</kbd>
 | 打开设备屏幕                                 | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | 旋转设备屏幕                                 | <kbd>MOD</kbd>+<kbd>r</kbd>
 | 展开通知面板                                 | <kbd>MOD</kbd>+<kbd>n</kbd>
 | 展开快捷操作                                 | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | 复制到剪贴板³                                | <kbd>MOD</kbd>+<kbd>c</kbd>
 | 剪切到剪贴板³                                | <kbd>MOD</kbd>+<kbd>x</kbd>
 | 同步剪贴板并黏贴³                            | <kbd>MOD</kbd>+<kbd>v</kbd>
 | 导入电脑剪贴板文本                           | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | 打开/关闭FPS显示（在 stdout)                 | <kbd>MOD</kbd>+<kbd>i</kbd>
 | 捏拉缩放                                    | <kbd>Ctrl</kbd>+_点按并移动鼠标_

_¹双击黑色边界以关闭黑色边界_  
_²点击鼠标右键将在屏幕熄灭时点亮屏幕，其余情况则视为按下 返回键 。_  
_³需要安卓版本 Android >= 7。_

所有的 <kbd>Ctrl</kbd>+_按键_ 的热键都是被转发到设备进行处理的，所以实际上会由当前应用程序对其做出响应。


## 自定义路径

为了使用您想使用的 _adb_ ，您可以在环境变量
`ADB`中设置它的路径：

    ADB=/path/to/adb scrcpy

如果需要覆盖`scrcpy-server`的路径，您可以在
`SCRCPY_SERVER_PATH`中设置它。

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## 为什么叫 _scrcpy_ ？

一个同事让我找出一个和[gnirehtet]一样难以发音的名字。

[`strcpy`] 可以复制**str**ing； `scrcpy` 可以复制**scr**een。

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## 如何编译？

请查看[编译]。

[编译]: BUILD.md


## 常见问题

请查看[FAQ](FAQ.md).


## 开发者

请查看[开发者页面]。

[开发者页面]: DEVELOP.md


## 许可协议

    Copyright (C) 2018 Genymobile
    Copyright (C) 2018-2020 Romain Vimont

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
