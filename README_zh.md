# scrcpy (v1.16)

本应用程序可以让你实现在电脑上对安卓设备的查看和操作。
该程序不要求 _root_ 权限，它只需要您的手机和电脑通过USB（或通过TCP/IP）连接。
该应用程序可以在 _GNU/Linux_, _Windows_ and _macOS_ 环境下运行

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


## 获取应用程序

<a href="https://repology.org/project/scrcpy/versions"><img src="https://repology.org/badge/vertical-allrepos/scrcpy.svg" alt="Packaging status" align="right"></a>

### Linux

在Debian (_testing_ and _sid_ for now) 和Ubuntu (20.04)上：

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

您也可以[自行编译][BUILD](不必担心，这并不困难)。



### Windows

在Windows上，简便起见，我们准备了包含所有依赖项（包括adb）的程序。

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

您也可以[自行编译][BUILD]。


### macOS

本应用程序可以使用[Homebrew]下载。直接安装就可以了：

[Homebrew]: https://brew.sh/

```bash
brew install scrcpy
```

您需要 `adb`，并且它应该可以通过 `PATH`被访问。如果您没有：

```bash
brew cask install android-platform-tools
```

您也可以[自行编译][BUILD]。


## 运行程序

用USB链接电脑和安卓设备，并执行：

```bash
scrcpy
```

本程序接受命令行输入，可以通过如下命令查看帮助：

```bash
scrcpy --help
```

## 功能介绍

### 画面设置

#### 缩小分辨率

有时候，将设备输出分辨率降低可以有效地提升性能。

我们可以将高度和宽度都限制在一定大小内（如 1024）：

```bash
scrcpy --max-size 1024
scrcpy -m 1024  # short version
```

较短的一边会被按比例缩小以保持设备的显示比例。
这样，1920x1080的设备会以1020x576的分辨率显示。


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

设备画面可在输出到电脑时被裁切，以仅显示屏幕的一部分。

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

可以在传输的同时录制视频：

```bash
scrcpy --record file.mp4
scrcpy -r file.mkv
```

在不开启屏幕共享的同时录制：

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

#### Autostart on device connection

You could use [AutoAdb]:

```bash
autoadb scrcpy -s '{}'
```

[AutoAdb]: https://github.com/rom1v/autoadb

#### SSH tunnel

To connect to a remote device, it is possible to connect a local `adb` client to
a remote `adb` server (provided they use the same version of the _adb_
protocol):

```bash
adb kill-server    # kill the local adb server on 5037
ssh -CN -L5037:localhost:5037 -R27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal:

```bash
scrcpy
```

To avoid enabling remote port forwarding, you could force a forward connection
instead (notice the `-L` instead of `-R`):

```bash
adb kill-server    # kill the local adb server on 5037
ssh -CN -L5037:localhost:5037 -L27183:localhost:27183 your_remote_computer
# keep this open
```

From another terminal:

```bash
scrcpy --force-adb-forward
```


Like for wireless connections, it may be useful to reduce quality:

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

您可以通过如下命令直接全屏打开共享：

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
_(left)_ and <kbd>MOD</kbd>+<kbd>→</kbd> _(right)_ 的快捷键实时更改。

Note that _scrcpy_ manages 3 different rotations:
- <kbd>MOD</kbd>+<kbd>r</kbd> requests the device to switch between portrait and
  landscape (the current running app may refuse, if it does support the
  requested orientation).
 - `--lock-video-orientation` changes the mirroring orientation (the orientation
   of the video sent from the device to the computer). This affects the
   recording.
 - `--rotation` (or <kbd>MOD</kbd>+<kbd>←</kbd>/<kbd>MOD</kbd>+<kbd>→</kbd>)
   rotates only the window content. This affects only the display, not the
   recording.


### Other mirroring options

#### Read-only

To disable controls (everything which can interact with the device: input keys,
mouse events, drag&drop files):

```bash
scrcpy --no-control
scrcpy -n
```

#### Display

If several displays are available, it is possible to select the display to
mirror:

```bash
scrcpy --display 1
```

The list of display ids can be retrieved by:

```
adb shell dumpsys display   # search "mDisplayId=" in the output
```

The secondary display may only be controlled if the device runs at least Android
10 (otherwise it is mirrored in read-only).


#### Stay awake

To prevent the device to sleep after some delay when the device is plugged in:

```bash
scrcpy --stay-awake
scrcpy -w
```

The initial state is restored when scrcpy is closed.


#### Turn screen off

It is possible to turn the device screen off while mirroring on start with a
command-line option:

```bash
scrcpy --turn-screen-off
scrcpy -S
```

Or by pressing <kbd>MOD</kbd>+<kbd>o</kbd> at any time.

To turn it back on, press <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>.

On Android, the `POWER` button always turns the screen on. For convenience, if
`POWER` is sent via scrcpy (via right-click or <kbd>MOD</kbd>+<kbd>p</kbd>), it
will force to turn the screen off after a small delay (on a best effort basis).
The physical `POWER` button will still cause the screen to be turned on.

It can also be useful to prevent the device from sleeping:

```bash
scrcpy --turn-screen-off --stay-awake
scrcpy -Sw
```


#### Render expired frames

By default, to minimize latency, _scrcpy_ always renders the last decoded frame
available, and drops any previous one.

To force the rendering of all frames (at a cost of a possible increased
latency), use:

```bash
scrcpy --render-expired-frames
```

#### Show touches

For presentations, it may be useful to show physical touches (on the physical
device).

Android provides this feature in _Developers options_.

_Scrcpy_ provides an option to enable this feature on start and restore the
initial value on exit:

```bash
scrcpy --show-touches
scrcpy -t
```

Note that it only shows _physical_ touches (with the finger on the device).


#### Disable screensaver

By default, scrcpy does not prevent the screensaver to run on the computer.

To disable it:

```bash
scrcpy --disable-screensaver
```


### Input control

#### Rotate device screen

Press <kbd>MOD</kbd>+<kbd>r</kbd> to switch between portrait and landscape
modes.

Note that it rotates only if the application in foreground supports the
requested orientation.

#### Copy-paste

Any time the Android clipboard changes, it is automatically synchronized to the
computer clipboard.

Any <kbd>Ctrl</kbd> shortcut is forwarded to the device. In particular:
 - <kbd>Ctrl</kbd>+<kbd>c</kbd> typically copies
 - <kbd>Ctrl</kbd>+<kbd>x</kbd> typically cuts
 - <kbd>Ctrl</kbd>+<kbd>v</kbd> typically pastes (after computer-to-device
   clipboard synchronization)

This typically works as you expect.

The actual behavior depends on the active application though. For example,
_Termux_ sends SIGINT on <kbd>Ctrl</kbd>+<kbd>c</kbd> instead, and _K-9 Mail_
composes a new message.

To copy, cut and paste in such cases (but only supported on Android >= 7):
 - <kbd>MOD</kbd>+<kbd>c</kbd> injects `COPY`
 - <kbd>MOD</kbd>+<kbd>x</kbd> injects `CUT`
 - <kbd>MOD</kbd>+<kbd>v</kbd> injects `PASTE` (after computer-to-device
   clipboard synchronization)

In addition, <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd> allows to inject the
computer clipboard text as a sequence of key events. This is useful when the
component does not accept text pasting (for example in _Termux_), but it can
break non-ASCII content.

**WARNING:** Pasting the computer clipboard to the device (either via
<kbd>Ctrl</kbd>+<kbd>v</kbd> or <kbd>MOD</kbd>+<kbd>v</kbd>) copies the content
into the device clipboard. As a consequence, any Android application could read
its content. You should avoid to paste sensitive content (like passwords) that
way.


#### Pinch-to-zoom

To simulate "pinch-to-zoom": <kbd>Ctrl</kbd>+_click-and-move_.

More precisely, hold <kbd>Ctrl</kbd> while pressing the left-click button. Until
the left-click button is released, all mouse movements scale and rotate the
content (if supported by the app) relative to the center of the screen.

Concretely, scrcpy generates additional touch events from a "virtual finger" at
a location inverted through the center of the screen.


#### Text injection preference

There are two kinds of [events][textevents] generated when typing text:
 - _key events_, signaling that a key is pressed or released;
 - _text events_, signaling that a text has been entered.

By default, letters are injected using key events, so that the keyboard behaves
as expected in games (typically for WASD keys).

But this may [cause issues][prefertext]. If you encounter such a problem, you
can avoid it by:

```bash
scrcpy --prefer-text
```

(but this will break keyboard behavior in games)

[textevents]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-text-input
[prefertext]: https://github.com/Genymobile/scrcpy/issues/650#issuecomment-512945343


#### Key repeat

By default, holding a key down generates repeated key events. This can cause
performance problems in some games, where these events are useless anyway.

To avoid forwarding repeated key events:

```bash
scrcpy --no-key-repeat
```


### File drop

#### Install APK

To install an APK, drag & drop an APK file (ending with `.apk`) to the _scrcpy_
window.

There is no visual feedback, a log is printed to the console.


#### Push file to device

To push a file to `/sdcard/` on the device, drag & drop a (non-APK) file to the
_scrcpy_ window.

There is no visual feedback, a log is printed to the console.

The target directory can be changed on start:

```bash
scrcpy --push-target /sdcard/foo/bar/
```


### Audio forwarding

Audio is not forwarded by _scrcpy_. Use [sndcpy].

Also see [issue #14].

[sndcpy]: https://github.com/rom1v/sndcpy
[issue #14]: https://github.com/Genymobile/scrcpy/issues/14


## Shortcuts

In the following list, <kbd>MOD</kbd> is the shortcut modifier. By default, it's
(left) <kbd>Alt</kbd> or (left) <kbd>Super</kbd>.

It can be changed using `--shortcut-mod`. Possible keys are `lctrl`, `rctrl`,
`lalt`, `ralt`, `lsuper` and `rsuper`. For example:

```bash
# use RCtrl for shortcuts
scrcpy --shortcut-mod=rctrl

# use either LCtrl+LAlt or LSuper for shortcuts
scrcpy --shortcut-mod=lctrl+lalt,lsuper
```

_<kbd>[Super]</kbd> is typically the <kbd>Windows</kbd> or <kbd>Cmd</kbd> key._

[Super]: https://en.wikipedia.org/wiki/Super_key_(keyboard_button)

 | Action                                      |   Shortcut
 | ------------------------------------------- |:-----------------------------
 | Switch fullscreen mode                      | <kbd>MOD</kbd>+<kbd>f</kbd>
 | Rotate display left                         | <kbd>MOD</kbd>+<kbd>←</kbd> _(left)_
 | Rotate display right                        | <kbd>MOD</kbd>+<kbd>→</kbd> _(right)_
 | Resize window to 1:1 (pixel-perfect)        | <kbd>MOD</kbd>+<kbd>g</kbd>
 | Resize window to remove black borders       | <kbd>MOD</kbd>+<kbd>w</kbd> \| _Double-click¹_
 | Click on `HOME`                             | <kbd>MOD</kbd>+<kbd>h</kbd> \| _Middle-click_
 | Click on `BACK`                             | <kbd>MOD</kbd>+<kbd>b</kbd> \| _Right-click²_
 | Click on `APP_SWITCH`                       | <kbd>MOD</kbd>+<kbd>s</kbd>
 | Click on `MENU` (unlock screen)             | <kbd>MOD</kbd>+<kbd>m</kbd>
 | Click on `VOLUME_UP`                        | <kbd>MOD</kbd>+<kbd>↑</kbd> _(up)_
 | Click on `VOLUME_DOWN`                      | <kbd>MOD</kbd>+<kbd>↓</kbd> _(down)_
 | Click on `POWER`                            | <kbd>MOD</kbd>+<kbd>p</kbd>
 | Power on                                    | _Right-click²_
 | Turn device screen off (keep mirroring)     | <kbd>MOD</kbd>+<kbd>o</kbd>
 | Turn device screen on                       | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>o</kbd>
 | Rotate device screen                        | <kbd>MOD</kbd>+<kbd>r</kbd>
 | Expand notification panel                   | <kbd>MOD</kbd>+<kbd>n</kbd>
 | Collapse notification panel                 | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>n</kbd>
 | Copy to clipboard³                          | <kbd>MOD</kbd>+<kbd>c</kbd>
 | Cut to clipboard³                           | <kbd>MOD</kbd>+<kbd>x</kbd>
 | Synchronize clipboards and paste³           | <kbd>MOD</kbd>+<kbd>v</kbd>
 | Inject computer clipboard text              | <kbd>MOD</kbd>+<kbd>Shift</kbd>+<kbd>v</kbd>
 | Enable/disable FPS counter (on stdout)      | <kbd>MOD</kbd>+<kbd>i</kbd>
 | Pinch-to-zoom                               | <kbd>Ctrl</kbd>+_click-and-move_

_¹Double-click on black borders to remove them._  
_²Right-click turns the screen on if it was off, presses BACK otherwise._  
_³Only on Android >= 7._

All <kbd>Ctrl</kbd>+_key_ shortcuts are forwarded to the device, so they are
handled by the active application.


## 自定义路径

为了使用您想使用的 _adb_ ，您可以在环境变量
`ADB`中设置它的路径：

    ADB=/path/to/adb scrcpy

如果需要覆盖`scrcpy-server`的路径，您可以在
`SCRCPY_SERVER_PATH`中设置它。

[useful]: https://github.com/Genymobile/scrcpy/issues/278#issuecomment-429330345


## 为什么叫 _scrcpy_ ？

一个同事让我找出一个和[gnirehtet]一样难以发音的名字。

[`strcpy`] 可以复制**str**ing; `scrcpy` 可以“复制”**scr**een。

[gnirehtet]: https://github.com/Genymobile/gnirehtet
[`strcpy`]: http://man7.org/linux/man-pages/man3/strcpy.3.html


## 如何编译？

请查看[编译]。

[编译]: BUILD.md


## 常见问题

请查看[FAQ](FAQ.md).


## 开发者

请查看[开发者页面]。

[开发者]: DEVELOP.md


## Licence

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

## Articles

- [Introducing scrcpy][article-intro]
- [Scrcpy now works wirelessly][article-tcpip]

[article-intro]: https://blog.rom1v.com/2018/03/introducing-scrcpy/
[article-tcpip]: https://www.genymotion.com/blog/open-source-project-scrcpy-now-works-wirelessly/
