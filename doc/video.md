# Video

## Source

By default, scrcpy mirrors the device screen.

It is possible to capture the device camera instead.

See the dedicated [camera](camera.md) page.


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

For camera mirroring, the `--max-size` value is used to select the camera source
size instead (among the available resolutions).


## Bit rate

The default video bit rate is 8 Mbps. To change it:

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

For advanced usage, to pass arbitrary parameters to the [`MediaFormat`],
check `--video-codec-options` in the manpage or in `scrcpy --help`.

[`MediaFormat`]: https://developer.android.com/reference/android/media/MediaFormat


## Encoder

Several encoders may be available on the device. They can be listed by:

```bash
scrcpy --list-encoders
```

Sometimes, the default encoder may have issues or even crash, so it is useful to
try another one:

```bash
scrcpy --video-codec=h264 --video-encoder=OMX.qcom.video.encoder.avc
```


## Orientation

The orientation may be applied at 3 different levels:
 - The [shortcut](shortcuts.md) <kbd>MOD</kbd>+<kbd>r</kbd> requests the
   device to switch between portrait and landscape (the current running app may
   refuse, if it does not support the requested orientation).
 - `--capture-orientation` changes the mirroring orientation (the orientation
   of the video sent from the device to the computer). This affects the
   recording.
 - `--orientation` is applied on the client side, and affects display and
   recording. For the display, it can be changed dynamically using
   [shortcuts](shortcuts.md).

To capture the video with a specific orientation:

```bash
scrcpy --capture-orientation=0
scrcpy --capture-orientation=90       # 90° clockwise
scrcpy --capture-orientation=180      # 180°
scrcpy --capture-orientation=270      # 270° clockwise
scrcpy --capture-orientation=flip0    # hflip
scrcpy --capture-orientation=flip90   # hflip + 90° clockwise
scrcpy --capture-orientation=flip180  # hflip + 180°
scrcpy --capture-orientation=flip270  # hflip + 270° clockwise
```

The capture orientation can be locked by using `@`, so that a physical device
rotation does not change the captured video orientation:

```bash
scrcpy --capture-orientation=@         # locked to the initial orientation
scrcpy --capture-orientation=@0        # locked to 0°
scrcpy --capture-orientation=@90       # locked to 90° clockwise
scrcpy --capture-orientation=@180      # locked to 180°
scrcpy --capture-orientation=@270      # locked to 270° clockwise
scrcpy --capture-orientation=@flip0    # locked to hflip
scrcpy --capture-orientation=@flip90   # locked to hflip + 90° clockwise
scrcpy --capture-orientation=@flip180  # locked to hflip + 180°
scrcpy --capture-orientation=@flip270  # locked to hflip + 270° clockwise
```

The capture orientation transform is applied after `--crop`, but before
`--angle`.

To orient the video (on the client side):

```bash
scrcpy --orientation=0
scrcpy --orientation=90       # 90° clockwise
scrcpy --orientation=180      # 180°
scrcpy --orientation=270      # 270° clockwise
scrcpy --orientation=flip0    # hflip
scrcpy --orientation=flip90   # hflip + 90° clockwise
scrcpy --orientation=flip180  # vflip (hflip + 180°)
scrcpy --orientation=flip270  # hflip + 270° clockwise
```

The orientation can be set separately for display and record if necessary, via
`--display-orientation` and `--record-orientation`.

The rotation is applied to a recorded file by writing a display transformation
to the MP4 or MKV target file. Flipping is not supported, so only the 4 first
values are allowed when recording.


## Angle

To rotate the video content by a custom angle (in degrees, clockwise):

```
scrcpy --angle=23
```

The center of rotation is the center of the visible area.

This transformation is applied after `--crop` and `--capture-orientation`.


## Crop

The device screen may be cropped to mirror only part of the screen.

This is useful, for example, to mirror only one eye of the Oculus Go:

```bash
scrcpy --crop=1224:1440:0:0   # 1224x1440 at offset (0,0)
```

The values are expressed in the device natural orientation (portrait for a
phone, landscape for a tablet).

Cropping is performed before `--capture-orientation` and `--angle`.

For display mirroring, `--max-size` is applied after cropping. For camera,
`--max-size` is applied first (because it selects the source size rather than
resizing the content).


## Display

If several displays are available on the Android device, it is possible to
select the display to mirror:

```bash
scrcpy --display-id=1
```

The list of display ids can be retrieved by:

```bash
scrcpy --list-displays
```

A secondary display may only be controlled if the device runs at least Android
10 (otherwise it is mirrored as read-only).

It is also possible to create a [virtual display](virtual_display.md).


## Buffering

By default, there is no video buffering, to get the lowest possible latency.

Buffering can be added to delay the video stream and compensate for jitter to
get a smoother playback (see [#2464]).

[#2464]: https://github.com/Genymobile/scrcpy/issues/2464

The configuration is available independently for the display,
[v4l2 sinks](video.md#video4linux) and [audio](audio.md#buffering) playback.

```bash
scrcpy --video-buffer=50     # add 50ms buffering for video playback
scrcpy --audio-buffer=200    # set 200ms buffering for audio playback
scrcpy --v4l2-buffer=300     # add 300ms buffering for v4l2 sink
```

They can be applied simultaneously:

```bash
scrcpy --video-buffer=50 --v4l2-buffer=300
```


## No playback

It is possible to capture an Android device without playing video or audio on
the computer. This option is useful when [recording](recording.md) or when
[v4l2](#video4linux) is enabled:

```bash
scrcpy --v4l2-sink=/dev/video2 --no-playback
scrcpy --record=file.mkv --no-playback
# interrupt with Ctrl+C
```

It is also possible to disable video and audio playback separately:

```bash
# Send video to V4L2 sink without playing it, but keep audio playback
scrcpy --v4l2-sink=/dev/video2 --no-video-playback

# Record both video and audio, but only play video
scrcpy --record=file.mkv --no-audio-playback
```


## No video

To disable video forwarding completely, so that only audio is forwarded:

```
scrcpy --no-video
```


## Video4Linux

See the dedicated [Video4Linux](v4l2.md) page.
