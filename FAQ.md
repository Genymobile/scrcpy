# Frequently Asked Questions

[Read in another language](#translations)

Here are the common reported problems and their status.


## `adb` issues

`scrcpy` execute `adb` commands to initialize the connection with the device. If
`adb` fails, then scrcpy will not work.

In that case, it will print this error:

>     ERROR: "adb push" returned with value 1

This is typically not a bug in _scrcpy_, but a problem in your environment.

To find out the cause, execute:

```bash
adb devices
```

### `adb` not found

You need `adb` accessible from your `PATH`.

On Windows, the current directory is in your `PATH`, and `adb.exe` is included
in the release, so it should work out-of-the-box.


### Device unauthorized

Check [stackoverflow][device-unauthorized].

[device-unauthorized]: https://stackoverflow.com/questions/23081263/adb-android-device-unauthorized


### Device not detected

>     adb: error: failed to get feature set: no devices/emulators found

Check that you correctly enabled [adb debugging][enable-adb].

If your device is not detected, you may need some [drivers] (on Windows).

[enable-adb]: https://developer.android.com/studio/command-line/adb.html#Enabling
[drivers]: https://developer.android.com/studio/run/oem-usb.html


### Several devices connected

If several devices are connected, you will encounter this error:

>     adb: error: failed to get feature set: more than one device/emulator

the identifier of the device you want to mirror must be provided:

```bash
scrcpy -s 01234567890abcdef
```

Note that if your device is connected over TCP/IP, you'll get this message:

>     adb: error: more than one device/emulator
>     ERROR: "adb reverse" returned with value 1
>     WARN: 'adb reverse' failed, fallback to 'adb forward'

This is expected (due to a bug on old Android versions, see [#5]), but in that
case, scrcpy fallbacks to a different method, which should work.

[#5]: https://github.com/Genymobile/scrcpy/issues/5


### Conflicts between adb versions

>     adb server version (41) doesn't match this client (39); killing...

This error occurs when you use several `adb` versions simultaneously. You must
find the program using a different `adb` version, and use the same `adb` version
everywhere.

You could overwrite the `adb` binary in the other program, or ask _scrcpy_ to
use a specific `adb` binary, by setting the `ADB` environment variable:

```bash
set ADB=/path/to/your/adb
scrcpy
```


### Device disconnected

If _scrcpy_ stops itself with the warning "Device disconnected", then the
`adb` connection has been closed.

Try with another USB cable or plug it into another USB port. See [#281] and
[#283].

[#281]: https://github.com/Genymobile/scrcpy/issues/281
[#283]: https://github.com/Genymobile/scrcpy/issues/283



## Control issues

### Mouse and keyboard do not work

On some devices, you may need to enable an option to allow [simulating input].
In developer options, enable:

> **USB debugging (Security settings)**  
> _Allow granting permissions and simulating input via USB debugging_

[simulating input]: https://github.com/Genymobile/scrcpy/issues/70#issuecomment-373286323


### Special characters do not work

Injecting text input is [limited to ASCII characters][text-input]. A trick
allows to also inject some [accented characters][accented-characters], but
that's all. See [#37].

[text-input]: https://github.com/Genymobile/scrcpy/issues?q=is%3Aopen+is%3Aissue+label%3Aunicode
[accented-characters]: https://blog.rom1v.com/2018/03/introducing-scrcpy/#handle-accented-characters
[#37]: https://github.com/Genymobile/scrcpy/issues/37


## Client issues

### The quality is low

If the definition of your client window is smaller than that of your device
screen, then you might get poor quality, especially visible on text (see [#40]).

[#40]: https://github.com/Genymobile/scrcpy/issues/40

To improve downscaling quality, trilinear filtering is enabled automatically
if the renderer is OpenGL and if it supports mipmapping.

On Windows, you might want to force OpenGL:

```
scrcpy --render-driver=opengl
```

You may also need to configure the [scaling behavior]:

> `scrcpy.exe` > Properties > Compatibility > Change high DPI settings >
> Override high DPI scaling behavior > Scaling performed by: _Application_.

[scaling behavior]: https://github.com/Genymobile/scrcpy/issues/40#issuecomment-424466723



### KWin compositor crashes

On Plasma Desktop, compositor is disabled while _scrcpy_ is running.

As a workaround, [disable "Block compositing"][kwin].

[kwin]: https://github.com/Genymobile/scrcpy/issues/114#issuecomment-378778613


## Crashes

### Exception

There may be many reasons. One common cause is that the hardware encoder of your
device is not able to encode at the given definition:

> ```
> ERROR: Exception on thread Thread[main,5,main]
> android.media.MediaCodec$CodecException: Error 0xfffffc0e
> ...
> Exit due to uncaughtException in main thread:
> ERROR: Could not open video stream
> INFO: Initial texture: 1080x2336
> ```

or

> ```
> ERROR: Exception on thread Thread[main,5,main]
> java.lang.IllegalStateException
>         at android.media.MediaCodec.native_dequeueOutputBuffer(Native Method)
> ```

Just try with a lower definition:

```
scrcpy -m 1920
scrcpy -m 1024
scrcpy -m 800
```

You could also try another [encoder](README.md#encoder).


## Command line on Windows

Some Windows users are not familiar with the command line. Here is how to open a
terminal and run `scrcpy` with arguments:

 1. Press <kbd>Windows</kbd>+<kbd>r</kbd>, this opens a dialog box.
 2. Type `cmd` and press <kbd>Enter</kbd>, this opens a terminal.
 3. Go to your _scrcpy_ directory, by typing (adapt the path):

    ```bat
    cd C:\Users\user\Downloads\scrcpy-win64-xxx
    ```

    and press <kbd>Enter</kbd>
 4. Type your command. For example:

    ```bat
    scrcpy --record file.mkv
    ```

If you plan to always use the same arguments, create a file `myscrcpy.bat`
(enable [show file extensions] to avoid confusion) in the `scrcpy` directory,
containing your command. For example:

```bat
scrcpy --prefer-text --turn-screen-off --stay-awake
```

Then just double-click on that file.

You could also edit (a copy of) `scrcpy-console.bat` or `scrcpy-noconsole.vbs`
to add some arguments.

[show file extensions]: https://www.howtogeek.com/205086/beginner-how-to-make-windows-show-file-extensions/


## Translations

This FAQ is available in other languages:

 - [Italiano (Italiano, `it`) - v1.17](FAQ.it.md)
 - [한국어 (Korean, `ko`) - v1.11](FAQ.ko.md)
