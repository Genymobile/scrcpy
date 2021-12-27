只有原版的[FAQ](FAQ.md)会保持更新。
本文根据[d6aaa5]翻译。

[d6aaa5]:https://github.com/Genymobile/scrcpy/blob/d6aaa5bf9aa3710660c683b6e3e0ed971ee44af5/FAQ.md

# 常见问题

这里是一些常见的问题以及他们的状态。

## `adb` 相关问题

`scrcpy` 执行 `adb` 命令来初始化和设备之间的连接。如果`adb` 执行失败了， scrcpy 就无法工作。

在这种情况中，将会输出这个错误:

>     ERROR: "adb push" returned with value 1

这通常不是 _scrcpy_ 的bug，而是你的环境的问题。

要找出原因，请执行以下操作：

```bash
adb devices
```

### 找不到`adb`


你的`PATH`中需要能访问到`adb`。

在Windows上，当前目录会包含在`PATH`中，并且`adb.exe`也包含在发行版中，因此它应该是开箱即用（直接解压就可以）的。


### 设备未授权

参见这里 [stackoverflow][device-unauthorized].

[device-unauthorized]: https://stackoverflow.com/questions/23081263/adb-android-device-unauthorized


### 未检测到设备

>     adb: error: failed to get feature set: no devices/emulators found

确认已经正确启用 [adb debugging][enable-adb].

如果你的设备没有被检测到，你可能需要一些[驱动][drivers] (在 Windows上).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling
[drivers]: https://developer.android.com/studio/run/oem-usb.html


### 已连接多个设备

如果连接了多个设备，您将遇到以下错误：

>     adb: error: failed to get feature set: more than one device/emulator

必须提供要镜像的设备的标识符：

```bash
scrcpy -s 01234567890abcdef
```

注意，如果你的设备是通过 TCP/IP 连接的， 你将会收到以下消息：

>     adb: error: more than one device/emulator
>     ERROR: "adb reverse" returned with value 1
>     WARN: 'adb reverse' failed, fallback to 'adb forward'

这是意料之中的 (由于旧版安卓的一个bug, 请参见 [#5])，但是在这种情况下，scrcpy会退回到另一种方法，这种方法应该可以起作用。

[#5]: https://github.com/Genymobile/scrcpy/issues/5


### adb版本之间冲突

>     adb server version (41) doesn't match this client (39); killing...

同时使用多个版本的`adb`时会发生此错误。你必须查找使用不同`adb`版本的程序，并在所有地方使用相同版本的`adb`。

你可以覆盖另一个程序中的`adb`二进制文件，或者通过设置`ADB`环境变量来让 _scrcpy_ 使用特定的`adb`二进制文件。

```bash
set ADB=/path/to/your/adb
scrcpy
```


### 设备断开连接

如果 _scrcpy_ 在警告“设备连接断开”的情况下自动中止，那就意味着`adb`连接已经断开了。
请尝试使用另一条USB线或者电脑上的另一个USB接口。
请参看 [#281] 和 [#283]。

[#281]: https://github.com/Genymobile/scrcpy/issues/281
[#283]: https://github.com/Genymobile/scrcpy/issues/283

## 控制相关问题

### 鼠标和键盘不起作用


在某些设备上，您可能需要启用一个选项以允许 [模拟输入][simulating input]。

在开发者选项中，打开：

> **USB调试 (安全设置)**  
> _允许通过USB调试修改权限或模拟点击_

[simulating input]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


### 特殊字符不起作用

可输入的文本[被限制为ASCII字符][text-input]。也可以用一些小技巧输入一些[带重音符号的字符][accented-characters]，但是仅此而已。参见[#37]。


[text-input]: https://github.com/Genymobile/scrcpy/issues?q=is%3Aopen+is%3Aissue+label%3Aunicode
[accented-characters]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-accented-characters
[#37]: https://github.com/Genymobile/scrcpy/issues/37


## 客户端相关问题

### 效果很差

如果你的客户端窗口分辨率比你的设备屏幕小，则可能出现效果差的问题，尤其是在文本上（参见 [#40]）。

[#40]: https://github.com/Genymobile/scrcpy/issues/40


为了提升降尺度的质量，如果渲染器是OpenGL并且支持mip映射，就会自动开启三线性过滤。

在Windows上，你可能希望强制使用OpenGL：

```
scrcpy --render-driver=opengl
```

你可能还需要配置[缩放行为][scaling behavior]：

> `scrcpy.exe` > Properties > Compatibility > Change high DPI settings >
> Override high DPI scaling behavior > Scaling performed by: _Application_.

[scaling behavior]: https://github.com/Genymobile/scrcpy/issues/40#issuecomment-424466723


### Wayland相关的问题

在Linux上，SDL默认使用x11。可以通过`SDL_VIDEODRIVER`环境变量来更改[视频驱动][video driver]：

[video driver]: https://wiki.libsdl.org/FAQUsingSDL#how_do_i_choose_a_specific_video_driver

```bash
export SDL_VIDEODRIVER=wayland
scrcpy
```

在一些发行版上 (至少包括 Fedora)， `libdecor` 包必须手动安装。

参见 [#2554] 和 [#2559]。

[#2554]: https://github.com/Genymobile/scrcpy/issues/2554
[#2559]: https://github.com/Genymobile/scrcpy/issues/2559


### KWin compositor 崩溃

在Plasma桌面中，当 _scrcpy_ 运行时，会禁用compositor。

一种解决方法是， [禁用 "Block compositing"][kwin].

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613


## 崩溃

### 异常
可能有很多原因。一个常见的原因是您的设备无法按给定清晰度进行编码：

> ```
> ERROR: Exception on thread Thread[main,5,main]
> android.media.MediaCodec$CodecException: Error 0xfffffc0e
> ...
> Exit due to uncaughtException in main thread:
> ERROR: Could not open video stream
> INFO: Initial texture: 1080x2336
> ```

或者

> ```
> ERROR: Exception on thread Thread[main,5,main]
> java.lang.IllegalStateException
>         at android.media.MediaCodec.native_dequeueOutputBuffer(Native Method)
> ```

请尝试使用更低的清晰度：

```
scrcpy -m 1920
scrcpy -m 1024
scrcpy -m 800
```

你也可以尝试另一种 [编码器](README.md#encoder)。


## Windows命令行

一些Windows用户不熟悉命令行。以下是如何打开终端并带参数执行`scrcpy`：

 1. 按下 <kbd>Windows</kbd>+<kbd>r</kbd>，打开一个对话框。
 2. 输入 `cmd` 并按 <kbd>Enter</kbd>，这样就打开了一个终端。
 3. 通过输入以下命令，切换到你的 _scrcpy_ 所在的目录 （根据你的实际位置修改路径）:

    ```bat
    cd C:\Users\user\Downloads\scrcpy-win64-xxx
    ```

    然后按 <kbd>Enter</kbd>
 4. 输入你的命令。比如:

    ```bat
    scrcpy --record file.mkv
    ```

如果你打算总是使用相同的参数，在`scrcpy`目录创建一个文件 `myscrcpy.bat`
（启用 [显示文件拓展名][show file extensions] 避免混淆），文件中包含你的命令。例如：

```bat
scrcpy --prefer-text --turn-screen-off --stay-awake
```

然后双击刚刚创建的文件。

你也可以编辑 `scrcpy-console.bat` 或者 `scrcpy-noconsole.vbs`（的副本）来添加参数。

[show file extensions]: https://www.howtogeek.com/205086/beginner-how-to-make-windows-show-file-extensions/
