# Audio

Audio forwarding is supported for devices with Android 11 or higher, and it is
enabled by default:

 - For **Android 12 or newer**, it works out-of-the-box.
 - For **Android 11**, you'll need to ensure that the device screen is unlocked
   when starting scrcpy. A fake popup will briefly appear to make the system
   think that the shell app is in the foreground. Without this, audio capture
   will fail.
 - For **Android 10 or earlier**, audio cannot be captured and is automatically
   disabled.

If audio capture fails, then mirroring continues with video only (since audio is
enabled by default, it is not acceptable to make scrcpy fail if it is not
available), unless `--require-audio` is set.


## No audio

To disable audio:

```
scrcpy --no-audio
```

To disable only the audio playback, see [no playback](video.md#no-playback).

## Audio only

To play audio only, disable video and control:

```bash
scrcpy --no-video --no-control
```

To play audio without a window:

```bash
# --no-video and --no-control are implied by --no-window
scrcpy --no-window
# interrupt with Ctrl+C
```

Without video, the audio latency is typically not critical, so it might be
interesting to add [buffering](#buffering) to minimize glitches:

```
scrcpy --no-video --audio-buffer=200
```

## Source

By default, the device audio output is forwarded.

It is possible to capture the device microphone instead:

```
scrcpy --audio-source=mic
```

For example, to use the device as a dictaphone and record a capture directly on
the computer:

```
scrcpy --audio-source=mic --no-video --no-playback --record=file.opus
```

### Duplication

An alternative device audio capture method is also available (only for Android
13 and above):

```
scrcpy --audio-source=playback
```

This audio source supports keeping the audio playing on the device while
mirroring, with `--audio-dup`:

```bash
scrcpy --audio-source=playback --audio-dup
# or simply:
scrcpy --audio-dup  # --audio-source=playback is implied
```

However, it requires Android 13, and Android apps can opt-out (so they are not
captured).


See [#4380](https://github.com/Genymobile/scrcpy/issues/4380).


## Codec

The audio codec can be selected. The possible values are `opus` (default),
`aac`, `flac` and `raw` (uncompressed PCM 16-bit LE):

```bash
scrcpy --audio-codec=opus  # default
scrcpy --audio-codec=aac
scrcpy --audio-codec=flac
scrcpy --audio-codec=raw
```

In particular, if you get the following error:

> Failed to initialize audio/opus, error 0xfffffffe

then your device has no Opus encoder: try `scrcpy --audio-codec=aac`.

For advanced usage, to pass arbitrary parameters to the [`MediaFormat`],
check `--audio-codec-options` in the manpage or in `scrcpy --help`.

For example, to change the [FLAC compression level]:

```bash
scrcpy --audio-codec=flac --audio-codec-options=flac-compression-level=8
```

[`MediaFormat`]: https://developer.android.com/reference/android/media/MediaFormat
[FLAC compression level]: https://developer.android.com/reference/android/media/MediaFormat#KEY_FLAC_COMPRESSION_LEVEL


## Encoder

Several encoders may be available on the device. They can be listed by:

```bash
scrcpy --list-encoders
```

To select a specific encoder:

```bash
scrcpy --audio-codec=opus --audio-encoder='c2.android.opus.encoder'
```


## Bit rate

The default audio bit rate is 128Kbps. To change it:

```bash
scrcpy --audio-bit-rate=64K
scrcpy --audio-bit-rate=64000  # equivalent
```

_This parameter does not apply to RAW audio codec (`--audio-codec=raw`)._


## Buffering

Audio buffering is unavoidable. It must be kept small enough so that the latency
is acceptable, but large enough to minimize buffer underrun (causing audio
glitches).

The default buffer size is set to 50ms. It can be adjusted:

```bash
scrcpy --audio-buffer=40   # smaller than default
scrcpy --audio-buffer=100  # higher than default
```

Note that this option changes the _target_ buffering. It is possible that this
target buffering might not be reached (on frequent buffer underflow typically).

If you don't interact with the device (to watch a video for example), a higher
latency (for both [video](video.md#buffering) and audio) might be preferable to
avoid glitches and smooth the playback:

```
scrcpy --video-buffer=200 --audio-buffer=200
```

It is also possible to configure another audio buffer (the audio output buffer),
by default set to 5ms. Don't change it, unless you get some [robotic and glitchy
sound][#3793]:

```bash
# Only if absolutely necessary
scrcpy --audio-output-buffer=10
```

[#3793]: https://github.com/Genymobile/scrcpy/issues/3793
