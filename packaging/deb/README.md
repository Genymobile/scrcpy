# scrcpy-auto Debian package

Build a self-contained `.deb` for `scrcpy-auto` that installs on a **clean,
offline Ubuntu host** with no extra packages.

## What's in the package

The client binary is **statically linked** against FFmpeg (with `swscale` + the
PNG encoder, which the fork's `--screencap` needs), SDL3, dav1d and libusb, so
it drags in almost nothing at runtime. On top of that the package bundles:

| Path | What |
|---|---|
| `/usr/bin/scrcpy-auto` | the client (static media deps) |
| `/usr/share/scrcpy-auto/scrcpy-auto-server` | Android server (pushed to the device) |
| `/usr/lib/scrcpy-auto/adb` | bundled adb; `postinst` symlinks `/usr/bin/adb` only if none exists |
| man page, icons, `.desktop`, bash/zsh completions | via `meson install` |

`Depends:` is limited to base-image libraries (libc, libgcc, zlib), so
`dpkg -i` works with **no network**. The `Recommends:` (X11/Wayland/GL/audio
libs) are only needed to actually show a mirror window on a desktop; SDL loads
them with `dlopen`, so their absence does not block install or `--version` /
`--help` / daemon use.

## Build

Needs only **Docker** on the build host (no local C toolchain). The build
container needs internet; the produced `.deb` does not.

```sh
packaging/deb/build-deb.sh --test
```

Output: `dist/scrcpy-auto_<version>_amd64.deb`. `--test` then installs it in a
pristine `ubuntu:24.04` container with networking disabled and runs it.

### Options

- `--output DIR` — where to drop the `.deb` (default `dist/`)
- `--image IMAGE` — build + test base image (default `ubuntu:24.04`)
- `--server prebuilt|none|/abs/path` — server source. `prebuilt` (default)
  fetches the pinned upstream v4.0 server (the fork's `server/src` is unmodified
  upstream, so it is functionally identical, only renamed). Pass an absolute
  path to embed a locally built fork server instead.
- `--no-adb` — do not bundle adb
- `--version VER` — override the package version string

## Test only

```sh
packaging/deb/test-deb.sh dist/scrcpy-auto_<version>_amd64.deb
```

## Notes

- v4l2 output is disabled in this build to keep the dependency surface minimal;
  everything else (mirror, daemon, control, screencap, USB/HID) is enabled.
- Actual on-screen mirroring needs a desktop session and a connected device;
  the offline container test only proves install + load + `--version`/`--help`.
