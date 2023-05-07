# Video

## Size

By default, scrcpy attempts to mirror at the Android device resolution.

It might be useful to mirror at a lower definition to increase performance. To
limit both width and height to some maximum value (here 1024):

```bash
scrcpy --max-size=1024
scrcpy -m 1024   # short version
```

The other dimension is computed so that the Android device aspect ratio is
preserved. That way, a device in 1920×1080 will be mirrored at 1024×576.

If encoding fails, scrcpy automatically tries again with a lower definition
(unless `--no-downsize-on-error` is enabled).


## Bit rate

The default video bit-rate is 8 Mbps. To change it:

```bash
scrcpy --video-bit-rate=2M
scrcpy --video-bit-rate=2000000  # equivalent
scrcpy -b 2M                     # short version
```


## Frame rate

The capture frame rate can be limited:

```bash
scrcpy --max-fps=15
```

The actual capture frame rate may be printed to the console:

```
scrcpy --print-fps
```

It may also be enabled or disabled at anytime with <kbd>MOD</kbd>+<kbd>i</kbd>
(see [shortcuts](shortcuts.md)).

The frame rate is intrinsically variable: a new frame is produced only when the
screen content changes. For example, if you play a fullscreen video at 24fps on
your device, you should not get more than 24 frames per second in scrcpy.


## Codec

The video codec can be selected. The possible values are `h264` (default),
`h265` and `av1`:

```bash
scrcpy --video-codec=h264  # default
scrcpy --video-codec=h265
scrcpy --video-codec=av1
```

H265 may provide better quality, but H264 should provide lower latency.
AV1 encoders are not common on current Android devices.

Several encoders may be available on the device. They can be listed by:

```bash
scrcpy --list-encoders
```

Sometimes, the default encoder may have issues or even crash, so it is useful to
try another one:

```bash
scrcpy --video-codec=h264 --video-encoder='OMX.qcom.video.encoder.avc'
```

For advanced usage, to pass arbitrary parameters to the [`MediaFormat`],
check `--video-codec-options` in the manpage or in `scrcpy --help`.

[`MediaFormat`]: https://developer.android.com/reference/android/media/MediaFormat


## Rotation

The rotation may be applied at 3 different levels:
 - The [shortcut](shortcuts.md) <kbd>MOD</kbd>+<kbd>r</kbd> requests the
   device to switch between portrait and landscape (the current running app may
   refuse, if it does not support the requested orientation).
 - `--lock-video-orientation` changes the mirroring orientation (the orientation
   of the video sent from the device to the computer). This affects the
   recording.
 - `--rotation` rotates only the window content. This only affects the display,
   not the recording. It may be changed dynamically at any time using the
   [shortcuts](shortcuts.md) <kbd>MOD</kbd>+<kbd>←</kbd> and
   <kbd>MOD</kbd>+<kbd>→</kbd>.

To lock the mirroring orientation:

```bash
scrcpy --lock-video-orientation     # initial (current) orientation
scrcpy --lock-video-orientation=0   # natural orientation
scrcpy --lock-video-orientation=1   # 90° counterclockwise
scrcpy --lock-video-orientation=2   # 180°
scrcpy --lock-video-orientation=3   # 90° clockwise
```

To set an initial window rotation:

```bash
scrcpy --rotation=0   # no rotation
scrcpy --rotation=1   # 90 degrees counterclockwise
scrcpy --rotation=2   # 180 degrees
scrcpy --rotation=3   # 90 degrees clockwise
```

## Crop

The device screen may be cropped to mirror only part of the screen.

This is useful, for example, to mirror only one eye of the Oculus Go:

```bash
scrcpy --crop=1224:1440:0:0   # 1224x1440 at offset (0,0)
```

The values are expressed in the device natural orientation (portrait for a
phone, landscape for a tablet).

If `--max-size` is also specified, resizing is applied after cropping.


## Buffering

By default, there is no video buffering, to get the lowest possible latency.

Buffering can be added to delay the video stream and compensate for jitter to
get a smoother playback (see [#2464]).

[#2464]: https://github.com/Genymobile/scrcpy/issues/2464

The configuration is available independently for the display,
[v4l2 sinks](video.md#video4linux) and [audio](audio.md#buffering) playback.

```bash
scrcpy --display-buffer=50   # add 50ms buffering for display
scrcpy --v4l2-buffer=300     # add 300ms buffering for v4l2 sink
scrcpy --audio-buffer=200    # set 200ms buffering for audio playback
```

They can be applied simultaneously:

```bash
scrcpy --display-buffer=50 --v4l2-buffer=300
```


## No mirror

It is possible to capture an Android device without displaying a mirroring
window. This option is available if either [recording](recording.md) or
[v4l2](#video4linux) is enabled:

```bash
scrcpy --v4l2-sink=/dev/video2 --no-mirror
scrcpy --record=file.mkv --no-mirror
```


## No video

To disable video forwarding completely, so that only audio is forwarded:

```
scrcpy --no-video
```


## Video4Linux

See the dedicated [Video4Linux](v4l2.md) page.
