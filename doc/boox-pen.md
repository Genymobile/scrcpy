# BOOX native pen input (experimental)

The experimental `--boox-pen=ROTATION` option connects SDL 3 pen input to the actual BOOX digitizer evdev stream. It reuses scrcpy's existing position/pressure control messages, reserving downstream pointer ID `-4` without adding a message type or field. Ordinary mouse and finger input continue through the Android SDK for UI navigation; they do not become raw pen strokes.

The option is disabled by default. It requires Android 10+, display 0, SDK mouse mode and exactly one idle, shell-writable `onyx_emp...Wacom` digitizer with valid X/Y/pressure/distance capabilities. It does not use root, add an APK/application, patch Notes or install a persistent service. A private `app_process` child from the same server archive owns the descriptor and survives parent termination long enough to release it, like scrcpy's existing cleanup process.

## Install and prepare

Build a matching client and Android server from the same source revision using
the [build instructions](build.md). An upstream precompiled server does not
contain this experimental backend. Building an archive does not update an
installed client/server pair. Installation and real-device tests are separate
steps; the automated host fixtures never open a digitizer.

Use a desktop session where SDL receives real pen events and pressure from the tablet/display driver. A mouse is not a pressure-capable substitute. SDL's synthetic pen-to-mouse/touch duplicates are disabled only in BOOX mode. Eraser and tilt are not implemented in this first candidate.

Before enabling input, use normal scrcpy/mouse navigation to create or select a disposable synthetic Notes notebook/page. Never test in an existing valuable note. Put the physical BOOX pen away, and do not use another writer or scrcpy session against the digitizer. The lock prevents other instances of this backend, not the physical stylus or unrelated tools.

## Automatic input mapping

Use the same scrcpy window/video-to-display coordinate path as normal touch:

    scrcpy --boox-pen=auto --no-audio

Only the final Android-display-to-evdev conversion differs. The writer reads bounded `dumpsys input` metadata, matches the freshly discovered event path to its EventHub and InputReader device, and inverts that device's `SurfaceOrientation`. For example, the tested NoteAir metadata reports orientation 1, giving an inverse rotation of 270 degrees. No physical hover calibration is needed for this supported mapping.

Automatic mode currently accepts only an orientation-aware direct touchscreen mapper with identity affine calibration, matching raw-axis ranges, and a full, unoffset internal display-0 viewport. Unknown formats, offsets, viewport scaling, nonidentity affine calibration, ambiguous devices and display mismatches fail before the digitizer is opened; there is no guessed fallback. This deliberately supports a bounded metadata format, not every BOOX firmware.

InputReader surface orientation and logical `DisplayInfo.rotation` belong to different coordinate systems and need not be equal. On the observed NoteAir, these are 1 and 0 respectively. Startup compares logical dimensions, not these orientation values. The captured display dimensions and rotation must remain unchanged during input. A detected change closes the backend; reconnect to rediscover the mapping. Metadata is sampled at startup, not continuously monitored for firmware-specific calibration changes.

## Explicit mapping override

For unsupported metadata formats, a separately verified explicit rotation remains available. There is no guessed calibration. `ROTATION` is one of `0`, `90`, `180`, `270`: it maps the current unrotated scrcpy display coordinates into the raw digitizer axes, after scrcpy's own video/crop/window mapping. Normalized coordinate transforms are:

| Value | Raw X | Raw Y |
|---|---|---|
| 0 | x | y |
| 90 | y | 1-x |
| 180 | 1-x | 1-y |
| 270 | 1-y | x |

For example, after selecting the synthetic page:

    scrcpy --boox-pen=90 --no-audio

`90` is an example, not a verified calibration for every BOOX/firmware/orientation. Confirm alignment with a short central stroke, then small strokes in separate quadrants of the disposable page. If the mapping is reflected rather than rotated, stop: mirrored-axis calibration is not implemented. Do not rotate the device during a session; the backend closes on a detected display rotation change. Restart to rediscover the automatic mapping, or reverify the explicit override.

## Manual acceptance

1. Confirm the installed package revision and that startup reports the actual discovered digitizer. A failed/ambiguous discovery or permission failure must terminate the session, without falling back to another injection method.
2. On the synthetic page, test pen down/move/up and pressure changes. Mouse navigation must remain UI-only. A second pen must not take over an active stroke.
3. Test focus loss/proximity loss and a controlled scrcpy disconnect during a stroke. Confirm no stuck contact and that the physical pen works again after scrcpy closes. Do not kill the private writer child itself during a stroke.
4. With normal drawing paused, hold still: after approximately one second without a new pen packet, the safety watchdog releases. Lift and recontact to start again; a late move cannot resurrect the released stroke.
5. Reopen only the same synthetic notebook. Export only that notebook as `.note` and verify native shape/point records, pressure variation and persistence. A changing screen image alone is not acceptance.

Automated host fixtures validate encoding, pressure, state transitions, EOF cleanup,
discovery, captured InputReader metadata and SDL pen tracking. These checks and
a successful Android build do not establish end-to-end ink, alignment, saved
notebook content, latency or hardware fidelity. Repeat the controlled synthetic
notebook acceptance above for each device/firmware and client/server revision.

## Failure boundaries

EOF, partial internal packets, timeout, normal close and controller errors attempt pressure/contact release and restoration of the captured idle X/Y/distance before releasing ownership. Startup itself does not inject events. No automatic reconnect/replay occurs after a writer error.

No software can guarantee restoration if the kernel refuses writes, the device disappears, power is lost, or the writer process itself is forcibly killed. Such errors are fatal and reported; do not continue drawing. This candidate's integrated parent/child lifecycle has been compiled and fixture-tested, but still requires the manual tablet checks above.

## Build

The standard [build instructions](build.md) remain the default. An additional
`server/scripts/build-standalone.sh ANDROID_JAR D8_JAR OUTPUT` helper accepts an
existing JDK, Android platform and build-tools installation. It generates AIDL
stubs, compiles the server sources, runs D8 and packages the dex archive loaded
by `app_process`, without downloading dependencies.

Run `python3 server/scripts/test-ln-streams.py` and
`bash server/scripts/test-boox-pen.sh` for JDK/Python host fixtures. Meson debug
builds include pen-state, actual controller congestion/dispatch and CLI tests.
Assertions must be enabled; a release build does not register these C tests.

## Queue safety and host measurements

The client accepts ordinary droppable messages, including pen MOVE and
pressure-only MOVE, while the total queue contains fewer than 60 messages.
Excess MOVE samples are dropped, not coalesced. Already accepted samples retain
FIFO order; stationary pressure changes are not filtered as redundant XY motion.
Only reserved pen DOWN/UP/CANCEL bypass this ordinary limit. If an incoming pen
boundary finds 128 or more queued messages, or cannot allocate its queue slot,
the controller stops and attempts to interrupt the control socket. Reconnect to
resume. This is a safety budget, not a measured latency threshold. Existing UHID
CREATE/DESTROY allocation behavior is unchanged and is not subject to this pen
budget. An in-flight send cannot be retracted. Disconnect cleanup and the worker
watchdog remain fallback; failed transport or device writes cannot guarantee
physical contact release.

The worker buffers input reads and reuses one encoder buffer per BooxPen
instance under the existing synchronization. Host fixtures verify exact event
bytes for both ABI layouts, rotations, pressure, release and failed-write retries.
A queued 32-packet host fixture uses 2 underlying input-stream reads rather than
257. A host ThreadMXBean probe with 100,000 warmup and 300,000 measured MOVE
samples observed approximately 112/160 allocated bytes per sample before encoder
reuse (32/64-bit event layouts), and zero measured bytes afterward. Output remains
one 96/144-byte frame write per sample. These are host counters, not Android ART
GC, CPU, battery or end-to-end latency measurements.

A separate host OpenJDK ProcessBuilder/strace probe observed 32 pipe writes of
24 bytes for 32 samples with either the current primitive writes plus flush or a
prepacked packet. Each sample was acknowledged before the next sample or EOF.
There was no pipe-write-count reduction on this host, so no parent packet-buffer
change was adopted. Android stream/syscall behavior remains unmeasured. The
per-event display geometry guard remains intact: removing it requires a tested,
authoritative display-change invalidation mechanism, not assumed Binder savings.
