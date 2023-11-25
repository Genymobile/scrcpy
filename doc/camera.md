# Camera

Camera mirroring is supported for devices with Android 12 or higher.

To capture the camera instead of the device screen:

```
scrcpy --video-source=camera
```

By default, it automatically switches [audio source](audio.md#source) to
microphone (as if `--audio-source=mic` were also passed).

```bash
scrcpy --video-source=display  # default is --audio-source=output
scrcpy --video-source=camera   # default is --audio-source=mic
scrcpy --video-source=display --audio-source=mic    # force display AND microphone
scrcpy --video-source=camera --audio-source=output  # force camera AND device audio output
```

Audio can be disabled:

```bash
# audio not captured at all
scrcpy --video-source=camera --no-audio
scrcpy --video-source=camera --no-audio --record=file.mp4

# audio captured and recorded, but not played
scrcpy --video-source=camera --no-audio-playback --record=file.mp4
```


## List

To list the cameras available (with their declared valid sizes and frame rates):

```
scrcpy --list-cameras
scrcpy --list-camera-sizes
```

_Note that the sizes and frame rates are declarative. They are not accurate on
all devices: some of them are declared but not supported, while some others are
not declared but supported._


## Selection

It is possible to pass an explicit camera id (as listed by `--list-cameras`):

```
scrcpy --video-source=camera --camera-id=0
```

Alternatively, the camera may be selected automatically:

```bash
scrcpy --video-source=camera                           # use the first camera
scrcpy --video-source=camera --camera-facing=front     # use the first front camera
scrcpy --video-source=camera --camera-facing=back      # use the first back camera
scrcpy --video-source=camera --camera-facing=external  # use the first external camera
```

If `--camera-id` is specified, then `--camera-facing` is forbidden (the id
already determines the camera):

```bash
scrcpy --video-source=camera --camera-id=0 --camera-facing=front  # error
```


### Size selection

It is possible to pass an explicit camera size:

```
scrcpy --video-source=camera --camera-size=1920x1080
```

The given size may be listed among the declared valid sizes
(`--list-camera-sizes`), but may also be anything else (some devices support
arbitrary sizes):

```
scrcpy --video-source=camera --camera-size=1840x444
```

Alternatively, a declared valid size (among the ones listed by
`list-camera-sizes`) may be selected automatically.

Two constraints are supported:
 - `-m`/`--max-size` (already used for display mirroring), for example `-m1920`;
 - `--camera-ar` to specify an aspect ratio (`<num>:<den>`, `<value>` or
   `sensor`).

Some examples:

```bash
scrcpy --video-source=camera                          # use the greatest width and the greatest associated height
scrcpy --video-source=camera -m1920                   # use the greatest width not above 1920 and the greatest associated height
scrcpy --video-source=camera --camera-ar=4:3          # use the greatest size with an aspect ratio of 4:3 (+/- 10%)
scrcpy --video-source=camera --camera-ar=1.6          # use the greatest size with an aspect ratio of 1.6 (+/- 10%)
scrcpy --video-source=camera --camera-ar=sensor       # use the greatest size with the aspect ratio of the camera sensor (+/- 10%)
scrcpy --video-source=camera -m1920 --camera-ar=16:9  # use the greatest width not above 1920 and the closest to 16:9 aspect ratio
```

If `--camera-size` is specified, then `-m`/`--max-size` and `--camera-ar` are
forbidden (the size is determined by the value given explicitly):

```bash
scrcpy --video-source=camera --camera-size=1920x1080 -m3000  # error
```


## Rotation

To rotate the captured video, use the [video orientation](video.md#orientation)
option:

```
scrcpy --video-source=camera --camera-size=1920x1080 --orientation=90
```


## Frame rate

By default, camera is captured at Android's default frame rate (30 fps).

To configure a different frame rate:

```
scrcpy --video-source=camera --camera-fps=60
```


## High speed capture

The Android camera API also supports a [high speed capture mode][high speed].

This mode is restricted to specific resolutions and frame rates, listed by
`--list-camera-sizes`.

```
scrcpy --video-source=camera --camera-size=1920x1080 --camera-fps=240
```

[high speed]: https://developer.android.com/reference/android/hardware/camera2/CameraConstrainedHighSpeedCaptureSession


## Brace expansion tip

All camera options start with `--camera-`, so if your shell supports it, you can
benefit from [brace expansion] (for example, it is supported _bash_ and _zsh_):

```bash
scrcpy --video-source=camera --camera-{facing=back,ar=16:9,high-speed,fps=120}
```

This will be expanded as:

```bash
scrcpy --video-source=camera --camera-facing=back --camera-ar=16:9 --camera-high-speed --camera-fps=120
```

[brace expansion]: https://www.gnu.org/software/bash/manual/html_node/Brace-Expansion.html


## Webcam

Combined with the [V4L2](v4l2.md) feature on Linux, the Android device camera
may be used as a webcam on the computer.
