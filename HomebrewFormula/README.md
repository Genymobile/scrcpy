# Installing scrcpy-auto with Homebrew (macOS)

`scrcpy-auto` is an automation-focused fork of [scrcpy](https://github.com/Genymobile/scrcpy).
This directory holds a Homebrew formula so you can install the client with a
single command. Everything — dependencies, the client build, and the device-side
`scrcpy-auto-server` — is handled automatically.

## Quick install (from the GitHub URL)

```sh
brew install https://raw.githubusercontent.com/fish-dapangyu-dev/scrcpy/feat/control/HomebrewFormula/scrcpy-auto.rb
```

That installs a pinned, reproducible build (the `revision` in the formula).
To build the very latest fork work instead:

```sh
brew install --HEAD https://raw.githubusercontent.com/fish-dapangyu-dev/scrcpy/feat/control/HomebrewFormula/scrcpy-auto.rb
```

Then verify:

```sh
scrcpy-auto --version
adb devices          # your device should be listed and "authorized"
```

## Install as a tap (optional, enables `brew install scrcpy-auto`)

Because the formula lives in `HomebrewFormula/` at the repo root, Homebrew can
tap this repository directly:

```sh
brew tap fish-dapangyu-dev/scrcpy https://github.com/fish-dapangyu-dev/scrcpy
brew install scrcpy-auto
brew upgrade scrcpy-auto
```

## What the formula does

| Piece | How it's handled |
|---|---|
| **Client** (`scrcpy-auto`) | Built from source with meson/ninja against Homebrew's SDL3, FFmpeg (incl. libswscale), and libusb. |
| **Device server** (`scrcpy-auto-server`) | **Not built** — building the `.jar` needs the Android SDK. The formula downloads the matching upstream release artifact (`scrcpy-server-v4.0`, SHA-256 pinned) and hands it to meson via `-Dprebuilt_server=…`, which installs it as `share/scrcpy-auto/scrcpy-auto-server`. |
| **adb** | Provided by the `android-platform-tools` cask, declared as a dependency. |
| **Server discovery** | The client looks for the server at `<prefix>/share/scrcpy-auto/scrcpy-auto-server` (baked in at build time). Override with `SCRCPY_SERVER_PATH` if needed. |

### Why the server is downloaded, not built

The client and the device server communicate over a versioned wire protocol, so
the server must **match the client version**. The fork does not modify the
server — it only renames the file on install — so the upstream `scrcpy-server-v4.0`
release artifact is byte-compatible. Building it locally would require a full
Android SDK + Gradle toolchain for no benefit, so the formula fetches the pinned
release instead. This is the same `prebuilt_server` mechanism documented in
`doc/build.md`.

## Requirements handled for you

- `meson`, `ninja`, `pkgconf` (build)
- `ffmpeg`, `sdl3`, `libusb` (runtime libraries)
- `android-platform-tools` cask (`adb`)

## First-time device setup

1. On the Android device: **Settings → Developer options → USB debugging** (on).
2. Connect via USB and accept the **"Allow USB debugging"** prompt.
3. `adb devices` should show the device as `device` (not `unauthorized`).

## Maintainer notes (cutting a new release)

The formula pins two things that must stay in lock-step:

1. **`revision`** (and `version`) — the fork source commit to build.
2. The **`server` resource** `url` + `sha256` — the matching
   `scrcpy-server-v<VER>` upstream release artifact.

When you merge new fork work or bump the base scrcpy version:

- Update `revision` to the new commit SHA (and `version` if it changed).
- If the base scrcpy version changed, point the `server` resource at the new
  `scrcpy-server-v<VER>` and update its `sha256` (shown on scrcpy's release
  page / `doc/build.md`).

A mismatched server ("daemon protocol mismatch" / connection failures) almost
always means these two drifted apart. For a cleaner stable source you can also
create a git tag and switch the formula `url` to `tag:`/`revision:`.

## Troubleshooting

- **`Error: ... cask ... android-platform-tools`** — if your Homebrew rejects a
  cask dependency from a formula, remove the `depends_on cask:` line and run
  `brew install --cask android-platform-tools` manually.
- **`ERROR: Could not find scrcpy-auto-server`** — set
  `SCRCPY_SERVER_PATH=$(brew --prefix)/share/scrcpy-auto/scrcpy-auto-server`.
- **`adb: no devices/emulators found`** — re-check USB debugging and the
  authorization prompt; try a different cable/port.
