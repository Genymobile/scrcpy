# Recording

To record video and audio streams while mirroring:

```bash
scrcpy --record=file.mp4
scrcpy -r file.mkv
```

To record only the video:

```bash
scrcpy --no-audio --record=file.mp4
```

_It is currently not possible to record only the audio._

To disable mirroring while recording:

```bash
scrcpy --no-display --record=file.mp4
scrcpy -Nr file.mkv
# interrupt recording with Ctrl+C
```

Timestamps are captured on the device, so [packet delay variation] does not
impact the recorded file, which is always clean (only if you use `--record` of
course, not if you capture your scrcpy window and audio output on the computer).

[packet delay variation]: https://en.wikipedia.org/wiki/Packet_delay_variation

The video and audio streams are encoded on the device, but are muxed on the
client side. Two formats (containers) are supported:
 - Matroska (`.mkv`)
 - MP4 (`.mp4`)

The container is automatically selected based on the filename.

It is also possible to explicitly select a container (in that case the filename
needs not end with `.mkv` or `.mp4`):

```
scrcpy --record=file --record-format=mkv
```
