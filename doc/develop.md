# scrcpy for developers

## Overview

This application is composed of two parts:
 - the server (`scrcpy-server`), to be executed on the device,
 - the client (the `scrcpy` binary), executed on the host computer.

The client is responsible to push the server to the device and start its
execution.

The client and the server establish communication using separate sockets for
video, audio and controls. Any of them may be disabled (but not all), so
there are 1, 2 or 3 socket(s).

The server initially sends the device name on the first socket (it is used for
the scrcpy window title), then each socket is used for its own purpose. All
reads and writes are performed from a dedicated thread for each socket, both on
the client and on the server.

If video is enabled, then the server sends a raw video stream (H.264 by default)
of the device screen, with some additional headers for each packet. The client
decodes the video frames, and displays them as soon as possible, without
buffering (unless `--video-buffer=delay` is specified) to minimize latency. The
client is not aware of the device rotation (which is handled by the server), it
just knows the dimensions of the video frames it receives.

Similarly, if audio is enabled, then the server sends a raw audio stream (OPUS
by default) of the device audio output (or the microphone if
`--audio-source=mic` is specified), with some additional headers for each
packet. The client decodes the stream, attempts to keep a minimal latency by
maintaining an average buffering. The [blog post][scrcpy2] of the scrcpy v2.0
release gives more details about the audio feature.

If control is enabled, then the client captures relevant keyboard and mouse
events, that it transmits to the server, which injects them to the device. This
is the only socket which is used in both direction: input events are sent from
the client to the device, and when the device clipboard changes, the new content
is sent from the device to the client to support seamless copy-paste.

[scrcpy2]: https://blog.rom1v.com/2023/03/scrcpy-2-0-with-audio/

Note that the client-server roles are expressed at the application level:

 - the server _serves_ video and audio streams, and handle requests from the
   client,
 - the client _controls_ the device through the server.

However, by default (when `--force-adb-forward` is not set), the roles are
reversed at the network level:

 - the client opens a server socket and listen on a port before starting the
   server,
 - the server connects to the client.

This role inversion guarantees that the connection will not fail due to race
conditions without polling.


## Server


### Privileges

Capturing the screen requires some privileges, which are granted to `shell`.

The server is a Java application (with a [`public static void main(String...
args)`][main] method), compiled against the Android framework, and executed as
`shell` on the Android device.

[main]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/Server.java#L193

To run such a Java application, the classes must be [_dexed_][dex] (typically,
to `classes.dex`). If `my.package.MainClass` is the main class, compiled to
`classes.dex`, pushed to the device in `/data/local/tmp`, then it can be run
with:

    adb shell CLASSPATH=/data/local/tmp/classes.dex app_process / my.package.MainClass

_The path `/data/local/tmp` is a good candidate to push the server, since it's
readable and writable by `shell`, but not world-writable, so a malicious
application may not replace the server just before the client executes it._

Instead of a raw _dex_ file, `app_process` accepts a _jar_ containing
`classes.dex` (e.g. an [APK]). For simplicity, and to benefit from the gradle
build system, the server is built to an (unsigned) APK (renamed to
`scrcpy-server.jar`).

[dex]: https://en.wikipedia.org/wiki/Dalvik_(software)
[apk]: https://en.wikipedia.org/wiki/Android_application_package


### Hidden methods

Although compiled against the Android framework, [hidden] methods and classes are
not directly accessible (and they may differ from one Android version to
another).

They can be called using reflection though. The communication with hidden
components is provided by [_wrappers_ classes][wrappers] and [aidl].

[hidden]: https://stackoverflow.com/a/31908373/1987178
[wrappers]: https://github.com/Genymobile/scrcpy/tree/master/server/src/main/java/com/genymobile/scrcpy/wrappers
[aidl]: https://github.com/Genymobile/scrcpy/tree/master/server/src/main/aidl



### Execution

The server is started by the client basically by executing the following
commands:

```bash
adb push scrcpy-server /data/local/tmp/scrcpy-server.jar
adb forward tcp:27183 localabstract:scrcpy
adb shell CLASSPATH=/data/local/tmp/scrcpy-server.jar app_process / com.genymobile.scrcpy.Server 2.1
```

The first argument (`2.1` in the example) is the client scrcpy version. The
server fails if the client and the server do not have the exact same version.
The protocol between the client and the server may change from version to
version (see [protocol](#protocol) below), and there is no backward or forward
compatibility (there is no point to use different client and server versions).
This check allows to detect misconfiguration (running an older or newer server
by mistake).

It is followed by any number of arguments, in the form of `key=value` pairs.
Their order is irrelevant. The possible keys and associated value types can be
found in the [server][server-options] and [client][client-options] code.

[server-options]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/Options.java#L181
[client-options]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/app/src/server.c#L226

For example, if we execute `scrcpy -m1920 --no-audio`, then the server
execution will look like this:

```bash
# scid is a random number to identify different clients running on the same device
adb shell CLASSPATH=/data/local/tmp/scrcpy-server.jar app_process / com.genymobile.scrcpy.Server 2.1 scid=12345678 log_level=info audio=false max_size=1920
```

### Components

When executed, its [`main()`][main] method is executed (on the "main" thread).
It parses the arguments, establishes the connection with the client and starts
the other "components":
 - the **video** streamer: it captures the video screen and send encoded video
   packets on the _video_ socket (from the _video_ thread).
 - the **audio** streamer: it uses several threads to capture raw packets,
   submits them to encoding and retrieve encoded packets, which it sends on the
   _audio_ socket.
 - the **controller**: it receives _control messages_ (typically input events)
   on the _control_ socket from one thread, and sends _device messages_ (e.g. to
   transmit the device clipboard content to the client) on the same _control
   socket_ from another thread. Thus, the _control_ socket is used in both
   directions (contrary to the _video_ and _audio_ sockets).


### Screen video encoding

The encoding is managed by [`ScreenEncoder`].

The video is encoded using the [`MediaCodec`] API. The codec encodes the content
of a `Surface` associated to the display, and writes the encoding packets to the
client (on the _video_ socket).

[`ScreenEncoder`]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java
[`MediaCodec`]: https://developer.android.com/reference/android/media/MediaCodec.html

On device rotation (or folding), the encoding session is [reset] and restarted.

New frames are produced only when changes occur on the surface. This avoids to
send unnecessary frames, but by default there might be drawbacks:

 - it does not send any frame on start if the device screen does not change,
 - after fast motion changes, the last frame may have poor quality.

Both problems are [solved][repeat] by the flag
[`KEY_REPEAT_PREVIOUS_FRAME_AFTER`][repeat-flag].

[reset]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java#L179
[rotation]: https://github.com/Genymobile/scrcpy/blob/ffe0417228fb78ab45b7ee4e202fc06fc8875bf3/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java#L90
[repeat]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java#L246-L247
[repeat-flag]: https://developer.android.com/reference/android/media/MediaFormat.html#KEY_REPEAT_PREVIOUS_FRAME_AFTER


### Audio encoding

Similarly, the audio is [captured] using an [`AudioRecord`], and [encoded] using
the [`MediaCodec`] asynchronous API.

More details are available on the [blog post][scrcpy2] introducing the audio feature.

[captured]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/AudioCapture.java
[encoded]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/AudioEncoder.java
[`AudioRecord`]: https://developer.android.com/reference/android/media/AudioRecord


### Input events injection

_Control messages_ are received from the client by the [`Controller`] (run in a
separate thread). There are several types of input events:
 - keycode (cf [`KeyEvent`]),
 - text (special characters may not be handled by keycodes directly),
 - mouse motion/click,
 - mouse scroll,
 - other commands (e.g. to switch the screen on or to copy the clipboard).

Some of them need to inject input events to the system. To do so, they use the
_hidden_ method [`InputManager.injectInputEvent()`] (exposed by the
[`InputManager` wrapper][inject-wrapper]).

[`Controller`]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/Controller.java
[`KeyEvent`]: https://developer.android.com/reference/android/view/KeyEvent.html
[`MotionEvent`]: https://developer.android.com/reference/android/view/MotionEvent.html
[`InputManager.injectInputEvent()`]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/wrappers/InputManager.java#L34
[inject-wrapper]: https://github.com/Genymobile/scrcpy/blob/ffe0417228fb78ab45b7ee4e202fc06fc8875bf3/server/src/main/java/com/genymobile/scrcpy/wrappers/InputManager.java#L27



## Client

The client relies on [SDL], which provides cross-platform API for UI, input
events, threading, etc.

The video and audio streams are decoded by [FFmpeg].

[SDL]: https://www.libsdl.org
[ffmpeg]: https://ffmpeg.org/


### Initialization

The client parses the command line arguments, then [runs one of two code
paths][run]:
 - scrcpy in "normal" mode ([`scrcpy.c`])
 - scrcpy in [OTG mode](otg.md) ([`scrcpy_otg.c`])

[run]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/app/src/main.c#L81-L82
[`scrcpy.c`]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/app/src/scrcpy.c#L292-L293
[`scrcpy_otg.c`]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/app/src/usb/scrcpy_otg.c#L51-L52

In the remaining of this document, we assume that the "normal" mode is used
(read the code for the OTG mode).

On startup, the client:
 - opens the _video_, _audio_ and _control_ sockets;
 - pushes and starts the server on the device;
 - initializes its components (demuxers, decoders, recorder…).


### Video and audio streams

Depending on the arguments passed to `scrcpy`, several components may be used.
Here is an overview of the video and audio components:

```
                                                 V4L2 sink
                                               /
                                       decoder
                                     /         \
        VIDEO -------------> demuxer             display
                                     \
                                       recorder
                                     /
        AUDIO -------------> demuxer
                                     \
                                       decoder --- audio player
```

The _demuxer_ is responsible to extract video and audio packets (read some
header, split the video stream into packets at correct boundaries, etc.).

The demuxed packets may be sent to a _decoder_ (one per stream, to produce
frames) and to a recorder (receiving both video and audio stream to record a
single file). The packets are encoded on the device (by `MediaCodec`), but when
recording, they are _muxed_ (asynchronously) into a container (MKV or MP4) on
the client side.

Video frames are sent to the screen/display to be rendered in the scrcpy window.
They may also be sent to a [V4L2 sink](v4l2.md).

Audio "frames" (an array of decoded samples) are sent to the audio player.


### Controller

The _controller_ is responsible to send _control messages_ to the device. It
runs in a separate thread, to avoid I/O on the main thread.

On SDL event, received on the main thread, the _input manager_ creates
appropriate _control messages_. It is responsible to convert SDL events to
Android events. It then pushes the _control messages_ to a queue hold by the
controller. On its own thread, the controller takes messages from the queue,
that it serializes and sends to the client.


## Protocol

The protocol between the client and the server must be considered _internal_: it
may (and will) change at any time for any reason. Everything may change (the
number of sockets, the order in which the sockets must be opened, the data
format on the wire…) from version to version. A client must always be run with a
matching server version.

This section documents the current protocol in scrcpy v2.1.

### Connection

Firstly, the client sets up an adb tunnel:

```bash
# By default, a reverse redirection: the computer listens, the device connects
adb reverse localabstract:scrcpy_<SCID> tcp:27183

# As a fallback (or if --force-adb forward is set), a forward redirection:
# the device listens, the computer connects
adb forward tcp:27183 localabstract:scrcpy_<SCID>
```

(`<SCID>` is a 31-bit random number, so that it does not fail when several
scrcpy instances start "at the same time" for the same device.)

Then, up to 3 sockets are opened, in that order:
 - a _video_ socket
 - an _audio_ socket
 - a _control_ socket

Each one may be disabled (respectively by `--no-video`, `--no-audio` and
`--no-control`, directly or indirectly). For example, if `--no-audio` is set,
then the _video_ socket is opened first, then the _control_ socket.

On the _first_ socket opened (whichever it is), if the tunnel is _forward_, then
a [dummy byte] is sent from the device to the client. This allows to detect a
connection error (the client connection does not fail as long as there is an adb
forward redirection, even if nothing is listening on the device side).

Still on this _first_ socket, the device sends some [metadata][device meta] to
the client (currently only the device name, used as the window title, but there
might be other fields in the future).

[dummy byte]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/DesktopConnection.java#L93
[device meta]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/DesktopConnection.java#L151

You can read the [client][client-connection] and [server][server-connection]
code for more details.

[client-connection]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/app/src/server.c#L465-L466
[server-connection]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/DesktopConnection.java#L63

Then each socket is used for its intended purpose.

### Video and audio

On the _video_ and _audio_ sockets, the device first sends some [codec
metadata]:
 - On the _video_ socket, 12 bytes:
   - the codec id (`u32`) (H264, H265 or AV1)
   - the initial video width (`u32`)
   - the initial video height (`u32`)
 - On the _audio_ socket, 4 bytes:
   - the codec id (`u32`) (OPUS, AAC or RAW)

[codec metadata]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/Streamer.java#L33-L51

Then each packet produced by `MediaCodec` is sent, prefixed by a 12-byte [frame
header]:
 - config packet flag (`u1`)
 - key frame flag (`u1`)
 - PTS (`u62`)
 - packet size (`u32`)

Here is a schema describing the frame header:

```
    [. . . . . . . .|. . . .]. . . . . . . . . . . . . . . ...
     <-------------> <-----> <-----------------------------...
           PTS        packet        raw packet
                       size
     <--------------------->
           frame header

The most significant bits of the PTS are used for packet flags:

     byte 7   byte 6   byte 5   byte 4   byte 3   byte 2   byte 1   byte 0
    CK...... ........ ........ ........ ........ ........ ........ ........
    ^^<------------------------------------------------------------------->
    ||                                PTS
    | `- key frame
     `-- config packet
```

[frame header]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/Streamer.java#L83


### Controls

Controls messages are sent via a custom binary protocol.

The only documentation for this protocol is the set of unit tests on both sides:
 - `ControlMessage` (from client to device): [serialization](https://github.com/Genymobile/scrcpy/blob/master/app/tests/test_control_msg_serialize.c) | [deserialization](https://github.com/Genymobile/scrcpy/blob/master/server/src/test/java/com/genymobile/scrcpy/ControlMessageReaderTest.java)
 - `DeviceMessage` (from device to client) [serialization](https://github.com/Genymobile/scrcpy/blob/master/server/src/test/java/com/genymobile/scrcpy/DeviceMessageWriterTest.java) | [deserialization](https://github.com/Genymobile/scrcpy/blob/master/app/tests/test_device_msg_deserialize.c)


## Standalone server

Although the server is designed to work for the scrcpy client, it can be used
with any client which uses the same protocol.

For simplicity, some [server-specific options] have been added to produce raw
streams easily:
 - `send_device_meta=false`: disable the device metata (in practice, the device
   name) sent on the _first_ socket
 - `send_frame_meta=false`: disable the 12-byte header for each packet
 - `send_dummy_byte`: disable the dummy byte sent on forward connections
 - `send_codec_meta`: disable the codec information (and initial device size for
   video)
 - `raw_stream`: disable all the above

[server-specific options]: https://github.com/Genymobile/scrcpy/blob/a3cdf1a6b86ea22786e1f7d09b9c202feabc6949/server/src/main/java/com/genymobile/scrcpy/Options.java#L309-L329

Concretely, here is how to expose a raw H.264 stream on a TCP socket:

```bash
adb push scrcpy-server-v2.1 /data/local/tmp/scrcpy-server-manual.jar
adb forward tcp:1234 localabstract:scrcpy
adb shell CLASSPATH=/data/local/tmp/scrcpy-server-manual.jar \
    app_process / com.genymobile.scrcpy.Server 2.1 \
    tunnel_forward=true audio=false control=false cleanup=false \
    raw_stream=true max_size=1920
```

As soon as a client connects over TCP on port 1234, the device will start
streaming the video. For example, VLC can play the video (although you will
experience a very high latency, more details [here][vlc-0latency]):

```
vlc -Idummy --demux=h264 --network-caching=0 tcp://localhost:1234
```

[vlc-0latency]: https://code.videolan.org/rom1v/vlc/-/merge_requests/20


## Hack

For more details, go read the code!

If you find a bug, or have an awesome idea to implement, please discuss and
contribute ;-)


### Debug the server

The server is pushed to the device by the client on startup.

To debug it, enable the server debugger during configuration:

```bash
meson setup x -Dserver_debugger=true
# or, if x is already configured
meson configure x -Dserver_debugger=true
```

Then recompile, and run scrcpy.

For Android < 11, it will start a debugger on port 5005 on the device and wait:
Redirect that port to the computer:

```bash
adb forward tcp:5005 tcp:5005
```

For Android >= 11, first find the listening port:

```bash
adb jdwp
# press Ctrl+C to interrupt
```

Then redirect the resulting PID:

```bash
adb forward tcp:5005 jdwp:XXXX  # replace XXXX
```

In Android Studio, _Run_ > _Debug_ > _Edit configurations..._ On the left, click
on `+`, _Remote_, and fill the form:

 - Host: `localhost`
 - Port: `5005`

Then click on _Debug_.
