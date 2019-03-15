# scrcpy for developers

## Overview

This application is composed of two parts:
 - the server (`scrcpy-server.jar`), to be executed on the device,
 - the client (the `scrcpy` binary), executed on the host computer.

The client is responsible to push the server to the device and start its
execution.

Once the client and the server are connected to each other, the server initially
sends device information (name and initial screen dimensions), then starts to
send a raw H.264 video stream of the device screen. The client decodes the video
frames, and display them as soon as possible, without buffering, to minimize
latency. The client is not aware of the device rotation (which is handled by the
server), it just knows the dimensions of the video frames.

The client captures relevant keyboard and mouse events, that it transmits to the
server, which injects them to the device.



## Server


### Privileges

Capturing the screen requires some privileges, which are granted to `shell`.

The server is a Java application (with a [`public static void main(String...
args)`][main] method), compiled against the Android framework, and executed as
`shell` on the Android device.

[main]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/Server.java#L100

To run such a Java application, the classes must be [_dexed_][dex] (typically,
to `classes.dex`). If `my.package.MainClass` is the main class, compiled to
`classes.dex`, pushed to the device in `/data/local/tmp`, then it can be run
with:

    adb shell CLASSPATH=/data/local/tmp/classes.dex \
        app_process / my.package.MainClass

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
[wrappers]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/wrappers
[aidl]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/aidl/android/view


### Threading

The server uses 2 threads:

 - the **main** thread, encoding and streaming the video to the client;
 - the **controller** thread, listening for _control events_ (typically,
   keyboard and mouse events) from the client.

Since the video encoding is typically hardware, there would be no benefit in
encoding and streaming in two different threads.


### Screen video encoding

The encoding is managed by [`ScreenEncoder`].

The video is encoded using the [`MediaCodec`] API. The codec takes its input
from a [surface] associated to the display, and writes the resulting H.264
stream to the provided output stream (the socket connected to the client).

[`ScreenEncoder`]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java
[`MediaCodec`]: https://developer.android.com/reference/android/media/MediaCodec.html
[surface]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java#L69-L70

On device [rotation], the codec, surface and display are reinitialized, and a
new video stream is produced.

New frames are produced only when changes occur on the surface. This is good
because it avoids to send unnecessary frames, but there are drawbacks:

 - it does not send any frame on start if the device screen does not change,
 - after fast motion changes, the last frame may have poor quality.

Both problems are [solved][repeat] by the flag
[`KEY_REPEAT_PREVIOUS_FRAME_AFTER`][repeat-flag].

[rotation]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java#L90
[repeat]:
https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/ScreenEncoder.java#L147-L148
[repeat-flag]: https://developer.android.com/reference/android/media/MediaFormat.html#KEY_REPEAT_PREVIOUS_FRAME_AFTER


### Input events injection

_Control events_ are received from the client by the [`EventController`] (run in
a separate thread). There are 5 types of input events:
 - keycode (cf [`KeyEvent`]),
 - text (special characters may not be handled by keycodes directly),
 - mouse motion/click,
 - mouse scroll,
 - custom command (e.g. to switch the screen on).

All of them may need to inject input events to the system. To do so, they use
the _hidden_ method [`InputManager.injectInputEvent`] (exposed by our
[`InputManager` wrapper][inject-wrapper]).

[`EventController`]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/EventController.java#L66
[`KeyEvent`]: https://developer.android.com/reference/android/view/KeyEvent.html
[`MotionEvent`]: https://developer.android.com/reference/android/view/MotionEvent.html
[`InputManager.injectInputEvent`]: https://android.googlesource.com/platform/frameworks/base/+/oreo-release/core/java/android/hardware/input/InputManager.java#857
[inject-wrapper]: https://github.com/Genymobile/scrcpy/blob/v1.8/server/src/main/java/com/genymobile/scrcpy/wrappers/InputManager.java#L27



## Client

The client relies on [SDL], which provides cross-platform API for UI, input
events, threading, etc.

The video stream is decoded by [libav] (FFmpeg).

[SDL]: https://www.libsdl.org
[libav]: https://www.libav.org/

### Initialization

On startup, in addition to _libav_ and _SDL_ initialization, the client must
push and start the server on the device, and open a socket so that they may
communicate.

Note that the client-server roles are expressed at the application level:

 - the server _serves_ video stream and handle requests from the client,
 - the client _controls_ the device through the server.

However, the roles are reversed at the network level:

 - the client opens a server socket and listen on a port before starting the
   server,
 - the server connects to the client.

This role inversion guarantees that the connection will not fail due to race
conditions, and avoids polling.

_(Note that over TCP/IP, the roles are not reversed, due to a bug in `adb
reverse`. See commit [1038bad] and [issue #5].)_

Once the server is connected, it sends the device information (name and initial
screen dimensions). Thus, the client may init the window and renderer, before
the first frame is available.

To minimize startup time, SDL initialization is performed while listening for
the connection from the server (see commit [90a46b4]).

[1038bad]: https://github.com/Genymobile/scrcpy/commit/1038bad3850f18717a048a4d5c0f8110e54ee172
[issue #5]: https://github.com/Genymobile/scrcpy/issues/5
[90a46b4]: https://github.com/Genymobile/scrcpy/commit/90a46b4c45637d083e877020d85ade52a9a5fa8e


### Threading

The client uses 3 threads:

 - the **main** thread, executing the SDL event loop,
 - the **stream** thread, receiving the video and used for decoding and
   recording,
 - the **controller** thread, sending _control events_ to the server.

In addition, another thread can be started if necessary to handle APK
installation or file push requests (via drag&drop on the main window).



### Stream

The video [stream] is received from the socket (connected to the server on the
device) in a separate thread.

If a [decoder] is present (i.e. `--no-display` is not set), then it uses _libav_
to decode the H.264 stream from the socket, and notifies the main thread when a
new frame is available.

There are two [frames][video_buffer] simultaneously in memory:
 - the **decoding** frame, written by the decoder from the decoder thread,
 - the **rendering** frame, rendered in a texture from the main thread.

When a new decoded frame is available, the decoder _swaps_ the decoding and
rendering frame (with proper synchronization). Thus, it immediatly starts
to decode a new frame while the main thread renders the last one.

If a [recorder] is present (i.e. `--record` is enabled), then its muxes the raw
H.264 packet to the output video file.

[stream]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/stream.h
[decoder]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/decoder.h
[video_buffer]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/video_buffer.h
[recorder]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/recorder.h

```
                                   +----------+      +----------+
                              ---> | decoder  | ---> |  screen  |
             +---------+     /     +----------+      +----------+
 socket ---> | stream  | ----
             +---------+     \     +----------+
                              ---> | recorder |
                                   +----------+
```

### Controller

The [controller] is responsible to send _control events_ to the device. It runs
in a separate thread, to avoid I/O on the main thread.

On SDL event, received on the main thread, the [input manager][inputmanager]
creates appropriate [_control events_][controlevent]. It is responsible to
convert SDL events to Android events (using [convert]). It pushes the _control
events_ to a blocking queue hold by the controller. On its own thread, the
controller takes events from the queue, that it serializes and sends to the
client.

[controller]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/controller.h
[controlevent]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/control_event.h
[inputmanager]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/input_manager.h
[convert]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/convert.h


### UI and event loop

Initialization, input events and rendering are all [managed][scrcpy] in the main
thread.

Events are handled in the [event loop], which either updates the [screen] or
delegates to the [input manager][inputmanager].

[scrcpy]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/scrcpy.c
[event loop]:
https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/scrcpy.c#L187
[screen]: https://github.com/Genymobile/scrcpy/blob/v1.8/app/src/screen.h


## Hack

For more details, go read the code!

If you find a bug, or have an awesome idea to implement, please discuss and
contribute ;-)
