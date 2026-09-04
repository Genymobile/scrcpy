# Recording

To record video and audio streams while mirroring:

```bash
slink --record=file.mp4
slink -r file.mkv
```

To record only the video:

```bash
slink --no-audio --record=file.mp4
```

To record only the audio:

```bash
slink --no-video --record=file.opus
slink --no-video --audio-codec=aac --record=file.aac
slink --no-video --audio-codec=flac --record=file.flac
slink --no-video --audio-codec=raw --record=file.wav
# .m4a/.mp4 and .mka/.mkv are also supported for opus, aac and flac
```

Timestamps are captured on the device, so [packet delay variation] does not
impact the recorded file, which is always clean (only if you use `--record` of
course, not if you capture your Slink window and audio output on the computer).

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation


## Format

The video and audio streams are encoded on the device, but are muxed on the
client side. Several formats (containers) are supported:
 - MP4 (`.mp4`, `.m4a`, `.aac`)
 - Matroska (`.mkv`, `.mka`)
 - OPUS (`.opus`)
 - FLAC (`.flac`)
 - WAV (`.wav`)

The container is automatically selected based on the filename.

It is also possible to explicitly select a container (in that case the filename
needs not end with a known extension):

```bash
slink --record=file --record-format=mkv
```


## Rotation

The video can be recorded rotated. See [video
orientation](video.md#orientation).


## No playback

To disable playback and control while recording:

```bash
slink --no-playback --no-control --record=file.mp4
```

It is also possible to disable video and audio playback separately:

```bash
# Record both video and audio, but only play video
slink --record=file.mkv --no-audio-playback
```

To also disable the window:

```bash
slink --no-playback --no-window --record=file.mp4
# interrupt recording with Ctrl+C
```

## Time limit

To limit the recording time:

```bash
slink --record=file.mkv --time-limit=20  # in seconds
```

The `--time-limit` option is not limited to recording, it also impacts simple
mirroring:

```bash
slink --time-limit=20
```
