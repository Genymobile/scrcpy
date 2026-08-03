# scrcpy IME sidecar

This optional APK provides Unicode text injection for `scrcpy --ime`.

The APK is deliberately independent from the scrcpy server. The server opens
an exported connection provider as the Android shell; the provider verifies
the Binder caller UID and returns one end of a private Unix socket pair. All
session handshake and UTF-8 text frames then use that persistent local socket.
This bootstrap avoids Android SELinux restrictions on direct shell-to-app
abstract-socket connections without adding a public scrcpy control message.

Build a debug APK:

```bash
ime/scripts/build.sh debug
```

For a release APK, set `SCRCPY_IME_STORE_FILE`,
`SCRCPY_IME_STORE_PASSWORD`, `SCRCPY_IME_KEY_ALIAS` and
`SCRCPY_IME_KEY_PASSWORD`, then run:

```bash
ime/scripts/build.sh release
```

Copy `ime/dist/scrcpy-ime.apk` next to a portable scrcpy executable, install it
to `${PREFIX}/share/scrcpy/scrcpy-ime.apk`, or set `SCRCPY_IME_PATH` to its
absolute path. Keep `scrcpyVersionName` and `scrcpyVersionCode` in sync with the
client version when building a customized release.
