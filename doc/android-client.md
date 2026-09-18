# Android controller

The `android-app` module is an experimental Android client for controlling a
second Android device over ADB TCP. It is useful when the controller and target
are both phones and a desktop computer is not available.

The controller app does not require the desktop `adb` executable at runtime. It
implements the ADB TCP transport, stores its RSA authentication key encrypted
with Android Keystore in private app storage, pushes the matching scrcpy server
to the target, and renders the target video with Android `MediaCodec`.

## Build

The module is included in the repository Gradle build. It requires an Android
SDK with platform 36 installed. The controller supports Android 6.0 (API 23)
and newer because it uses Android Keystore to protect the ADB authentication
key.

On Linux or macOS:

```sh
./gradlew :android-app:assembleDebug
```

On Windows PowerShell:

```powershell
.\gradlew.bat :android-app:assembleDebug
```

Set `ANDROID_HOME` or `ANDROID_SDK_ROOT` if the SDK is not installed in the
default location. The debug APK is written to
`android-app/build/outputs/apk/debug/android-app-debug.apk`.

For a release build, use:

```sh
./gradlew :android-app:assembleRelease
```

The release variant enables R8 shrinking and resource shrinking. It is not
signed by this module, so configure a private signing key in your local or CI
Gradle configuration before distributing it.

The server APK is copied into the controller APK as a build artifact. Do not
check generated files from `build/` into the repository.

## Verification

The Android module keeps its JVM safety and lifecycle tests under
`android-app/src/test`. It also has a small instrumentation smoke test under
`android-app/src/androidTest` that launches the main activity and opens the
connection editor. The Android workflow runs the JVM tests, lint, the R8
release build, and that smoke test on a disposable API 35 Google APIs emulator.

Run the local checks with:

```sh
./gradlew :android-app:testDebugUnitTest :android-app:lintDebug :android-app:assembleRelease
```

With an emulator already running, run the instrumentation test with:

```sh
./gradlew :android-app:connectedDebugAndroidTest
```

## Setup

1. Enable ADB over TCP on the target device, using port 5555 or another port
   exposed by `adbd`.
2. Install the debug APK on the separate controller device.
3. Add a connection in the app with the target IP address and ADB port.
4. Read and acknowledge the first-use trusted-network warning before connecting.
5. Approve the controller's RSA key on the target while the target is unlocked.
6. Optionally enter an Android package name to launch that app automatically
   after connecting.
7. In the connection editor, enable Automatic resolution for adaptive video.
   The client keeps the ADB session and control channel alive while it lowers
   or raises the target video size when packet delivery becomes slow or stable.
   Disable it to select a fixed maximum video size.

The connection editor explains the network and authorization requirements. Do
not use an ADB TCP endpoint on a network you do not trust. The target address,
port, and optional package can be edited from the visible actions button on a
saved connection.

If the controller's ADB key becomes unavailable or the target should forget
this controller, use `Reset ADB authorization` on the connection list. The app
asks for confirmation, deletes the old encrypted key and its Keystore wrapping
alias, generates a new key, and requires authorization on the target again.

The app tears down the session when its video surface is destroyed or when it
goes into the background. Reconnect from the connection list after returning
to the app. A failed connection is closed before the UI returns to the list.

The controller and target must be reachable on the same trusted network. ADB
TCP is not encrypted and should not be exposed to an untrusted network.

## Controls

The connection list supports multiple saved targets. A normal tap connects to a
target, and the visible actions button opens options for connecting, editing,
or deleting a profile. A long press remains available as an alternate gesture.

While connected, the app provides:

- touch input on the target video;
- a clearly labelled Target back input;
- an Open app picker or direct package launch;
- disconnect, which closes the scrcpy session and requests the target display
  to turn off.

The app can reconnect while the target display is locked after the controller
key has been authorized. It cannot bypass a secure PIN, password, or pattern.
Launching an app while the target is locked therefore depends on the target
Android version and that app's lock-screen behavior.

## Scope and limitations

This client currently provides video and basic input control. It does not yet
provide audio forwarding, clipboard synchronization, recording, or the full
desktop scrcpy option set. The desktop clients remain the reference
implementation for those features.
