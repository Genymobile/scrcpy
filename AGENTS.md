# AGENTS.md

Context for AI coding agents working on _scrcpy_. Humans should start with
[README.md](README.md); this file covers the build, test and style details an
agent needs in order to make a change that will actually compile and be
accepted.

_scrcpy_ is two programs in one repository:

 - `app/`: the **client**, C11, built with Meson/Ninja, depends on SDL3,
   FFmpeg and libusb. This is the `scrcpy` binary that runs on the computer.
 - `server/`: the **server**, Java, built with Gradle against the Android
   framework, packaged as `scrcpy-server`. The client pushes it to the device
   and runs it there as `shell`.

They talk over adb-tunnelled sockets using a private binary protocol. Read
[doc/develop.md](doc/develop.md) before changing anything in either half.


## Branches

 - `master`: the latest release. Do not base work here.
 - `dev`: the development branch. **Base all commits and pull requests on
   `dev`.**


## Build

Requires `adb` on the `PATH`.

Building only the client is usually what you want: the server binary is
architecture-independent, so you can use a prebuilt one and skip needing Java
and the Android SDK.

```bash
# Client only, against a prebuilt server (fastest, no JDK needed)
meson setup x -Dprebuilt_server=/path/to/scrcpy-server
ninja -Cx          # DO NOT RUN AS ROOT

# Everything from source (needs ANDROID_SDK_ROOT set and a JDK)
meson setup x --buildtype=release --strip -Db_lto=true
ninja -Cx          # DO NOT RUN AS ROOT
```

Reconfigure an existing build dir with `meson configure x -D<option>=<value>`
rather than re-running `meson setup`.

Run without installing. This is the correct way to test a local build, because
it points the client at the server you just built:

```bash
./run x [options]
```

Useful `meson_options.txt` switches: `compile_server=false` (client only),
`prebuilt_server=<path>`, `server_debugger=true`, `portable=true`,
`v4l2`/`usb` (Linux-only features, on by default where supported).


## Test

These are the exact scripts CI runs. Make them pass before proposing a change.

```bash
release/test_client.sh    # C unit tests, built with ASan + UBSan
release/test_server.sh    # gradle -p server check (JUnit + checkstyle)
```

`test_client.sh` is equivalent to configuring with
`-Dcompile_server=false -Db_sanitize=address,undefined` and running
`ninja -C <builddir> test`. To iterate faster on a single test, build normally
and run `meson test -Cx test_str` (test names are listed at the bottom of
`app/meson.build`).

CI also verifies the server builds without Gradle:

```bash
server/build_without_gradle.sh
```

C tests live in `app/tests/`, Java tests in `server/src/test/java/`. A new C
test must be registered in the `tests` list in `app/meson.build`, along with
every source file it links.


## Testing against a device

The unit tests are headless and pure. **Everything else (video, audio,
control, recording, rotation) requires a real Android device reachable over
adb.** There is no fake device or mock transport in this repository, so a
change to those paths cannot be verified by running the test suite alone.

Any adb-reachable device works, including network devices:

```bash
adb devices                 # list serials
./run x -s <serial>         # target one explicitly
```

In rough order of fidelity:

 - **A physical device**, over USB or TCP/IP. Required for anything touching
   USB, OTG (`--otg`) or HID, and the only way to trust real encoder
   behaviour.
 - **The Android SDK emulator** (`emulator -avd <name>`, `-no-window` for
   headless). Free and adequate for most control, CLI and window work. Codec
   behaviour differs from real hardware, so it is a poor choice for
   reproducing encoder bugs.
 - **A cloud device.** Genymotion SaaS instances expose an ordinary adb
   serial, so scrcpy treats them like any other network device:

   ```bash
   gmsaas --format json instances adbconnect <instance_uuid>
   # -> {"adb_serial": "localhost:PORT"}
   ./run x -s localhost:PORT
   ```

   This is the practical option when the agent is running in a container or
   CI job with no device attached, and for checking behaviour across Android
   versions without maintaining hardware. Instances bill until stopped
   (`gmsaas instances stop <uuid>`).

If you cannot test a behavioural change on a device, say so explicitly in the
pull request rather than implying it was verified.

Note: `--stay-awake` has no effect on a device that is not physically plugged
in. That is Android behaviour, not a scrcpy bug. See
[doc/device.md](doc/device.md).


## Code style

**C (`app/`)**

 - C11. Indent 4 spaces, no tabs. Hard wrap at 80 columns.
 - All public functions and types are prefixed `sc_` (`sc_strncpy`,
   `sc_vecdeque`). Static helpers are not.
 - In definitions, the return type goes on its own line above the function
   name:

   ```c
   size_t
   sc_str_join(char *dst, const char *const tokens[], char sep, size_t n) {
   ```

 - Opening brace on the same line. Single-statement bodies frequently omit
   braces; match the surrounding file.
 - `goto` for cleanup and error paths is idiomatic here, not a smell.
 - Preprocessor directives nested inside conditionals are indented after the
   `#` (`# include <windows.h>`).
 - Platform-specific code goes behind `compat.h` or under `app/src/sys/`,
   never inline `#ifdef _WIN32` scattered through logic.

**Java (`server/`)**

 - Checkstyle runs as part of `gradle check` (config in `config/checkstyle/`),
   but with `ignoreFailures = true`, so a violation does **not** fail the
   build or CI. Read the report under `server/build/reports/checkstyle/` and
   fix violations yourself; do not rely on the build to catch them.
 - Hidden Android APIs are never called directly. They go through the
   reflection wrappers in `server/src/main/java/com/genymobile/scrcpy/wrappers/`
   or through aidl. If you need a new hidden method, add a wrapper.
 - Android version differences are handled in `AndroidVersions.java` and
   `Workarounds.java`.


## Commits and pull requests

Commit subjects are short, imperative, capitalised, with no scope prefix and
no trailing period. Conventional Commits are **not** used here:

```
Fix data race
Add --no-terminal-title
Reject recording VP8 in MP4
Bump version to 4.1
```

Keep commits small and single-purpose. This project splits refactors out from
behaviour changes as separate commits ("Extract function to dequeue events",
then the change that uses it). Do that rather than shipping one large commit.

Base the branch on `dev`.


## Gotchas that will waste your time

1. **Client and server versions must match exactly.** The server refuses to
   start otherwise. If you build the client from `dev` and point it at a
   prebuilt server from a release, it will fail with a version error. This is
   the single most common self-inflicted build problem.

2. **`./run x` is not the same as `scrcpy`.** The `run` script sets
   `SCRCPY_SERVER_PATH` to the server in your build dir. If you invoke an
   installed `scrcpy` binary after building, you are testing the installed
   server, not yours.

3. **Never run `ninja` as root.** Only `ninja install` needs root. Running the
   build as root leaves a build dir you cannot use afterwards.

4. **The client/server protocol is internal and deliberately unversioned.** It
   changes freely between releases. Do not add backward-compatibility shims or
   try to preserve wire format; there is no compatibility guarantee to
   maintain.

5. **The control protocol has no prose specification.** Its only documentation
   is the paired unit tests: `app/tests/test_control_msg_serialize.c` against
   `ControlMessageReaderTest.java`, and `DeviceMessageWriterTest.java` against
   `app/tests/test_device_msg_deserialize.c`. Change one side and you must
   change all four.

6. **The client is not aware of device rotation.** The server handles it and
   restarts the encoding session, sending a new session packet. Do not add
   rotation logic to the client.

7. `-Db_sanitize=address,undefined` is what CI uses for tests. A change that
   passes a plain build but trips ASan will fail CI.


## Where to read more

 - [doc/develop.md](doc/develop.md): architecture, protocol, server internals
 - [doc/build.md](doc/build.md): full per-platform build instructions
 - [FAQ.md](FAQ.md): known device and platform issues
 - `doc/` also has focused pages for audio, video, camera, control, keyboard,
   mouse, gamepad, recording, connection, tunnels, OTG, V4L2, window and
   [virtual displays](doc/virtual-display.md). Check the relevant one before
   changing that subsystem.
