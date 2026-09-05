# scrcpy-auto: how this fork differs from upstream scrcpy

This document is the reference for anyone (human or AI assistant) merging
upstream scrcpy into this fork or reviewing conflict resolutions. Its goal is
that no fork feature is ever silently lost or broken by an upstream merge.

For quick merge rules, see the repo-root `CLAUDE.md`. For the daemon design,
see `doc/daemon.md`.

---

## 1. Branch model

- `master` — mirrors upstream `Genymobile/scrcpy` `master`, no fork commits.
  Sync it first, then merge `master` into `feat/control`.
- `feat/control` — carries **all** fork changes. This is the branch that is
  built and shipped.

Fork baseline at the time of writing: upstream scrcpy **4.1**
(SDL 3.4.12, FFmpeg 8.1.2).

## 2. What the fork adds (three areas)

### 2.1 Rename: `scrcpy` → `scrcpy-auto`

Everything installable or device-visible is renamed so the fork can coexist
with an official scrcpy installation on the same machine **and** the same
device:

| What | Upstream | Fork |
|---|---|---|
| Client executable | `scrcpy` | `scrcpy-auto` |
| Server build output | `scrcpy-server` | `scrcpy-auto-server` |
| Server install dir | `share/scrcpy/` | `share/scrcpy-auto/` |
| Server path on device | `/data/local/tmp/scrcpy-server.jar` | `/data/local/tmp/scrcpy-auto-server.jar` |
| Manpage | `scrcpy.1` | `scrcpy-auto.1` |
| Icons | `scrcpy.png`, `disconnected.png` | `scrcpy-auto.png`, `scrcpy-auto-disconnected.png` |
| Desktop entries | `scrcpy(-console).desktop` | `scrcpy-auto(-console).desktop` |
| Completions | `scrcpy`, `_scrcpy` | `scrcpy-auto`, `_scrcpy-auto` |
| SDL app name | `scrcpy` | `scrcpy-auto` (`sdl_hints.c`) |

The load-bearing defines live in `app/src/server.c`:
`SC_SERVER_FILENAME`, `SC_SERVER_PATH_DEFAULT`, `SC_DEVICE_SERVER_PATH`.

### 2.2 One-shot automation options

- `--screencap <file.png>` — take a screenshot from the video stream and
  exit (`app/src/screencap.{c,h}`, a packet sink that decodes the first
  keyframe; PNG encoding via `sc_frame_to_png()`).
- `--control <command>` — inject input and exit; mini-language
  `click x y [ms]`, `swipe x1 y1 x2 y2 [ms]`, `input <text>`, `sleep ms`,
  chains with `&&`, repeatable for multi-finger gestures
  (`app/src/control_exec.{c,h}`; step durations are capped at 60 s).

### 2.3 Persistent daemon mode

`--daemon-port` / `--client-port` / `--daemon-host` (see `doc/daemon.md` for the
full spec): a per-device daemon keeps the device session alive and serves
screenshot/control requests from thin clients over a length-prefixed JSON
protocol, with reconnection supervision and an on-disk registry. `--daemon-host`
(default `127.0.0.1`) lets a client reach a remote daemon; `--json` makes the
client emit machine-readable output (a plugin's result object, or an error
object). Protocol v4 streams all five scrcpy 4.1 video codecs (`h264`, `h265`,
`av1`, `vp8`, `vp9`), carries camera/flex-display session metadata and exposes
codec/container/MIME metadata for clips and reports. VP8 clip/report files use
WebM; the other four codecs use MP4. All daemon code lives under
`app/src/daemon/`.

The fork remains additive: without daemon/client/automation flags, execution
uses the normal upstream scrcpy 4.1 path and retains its full feature set.
Daemon mode is intentionally headless and video/control oriented: audio,
`--record`, OTG/AOA, V4L2, listing and app-launch are normal-session features.
Camera daemon streams accept torch/zoom rather than screen input, and
flex-display streams cannot use the single-file report recorder or clip
extractor.

## 3. File inventory

### 3.1 Fork-owned files (do not exist upstream)

Merges never conflict on these, but upstream **API changes can still break
them silently** — see §5.

```
app/src/control_exec.{c,h}      --control executors (moved out of scrcpy.c)
app/src/screencap.{c,h}         --screencap sink + sc_frame_to_png()
app/src/plugins.{c,h}           plugin install/upgrade + --add-on name
                                resolution (doc/plugins.md); also
                                dispatched in main.c and resolved in
                                daemon/addon.c
app/src/daemon/daemon.{c,h}     daemon: listener, dispatch, session supervisor
app/src/daemon/client.{c,h}     thin client mode
app/src/daemon/protocol.{c,h}   IPC framing + minimal JSON (uses jsmn)
app/src/daemon/codec.{c,h}      exact enum/FFmpeg/wire codec-name mapping for
                                h264, h265, av1, vp8 and vp9
app/src/daemon/frame_keeper.{c,h} latest-frame sink for screenshots
app/src/daemon/broadcaster.{c,h}  protocol-v4 encoded-video push sink; emits
                                  fresh video_meta per stream session and
                                  permits config-less VP8/VP9 starts; shared
                                  immutable payloads and bounded per-client
                                  queues isolate slow browsers; a complete
                                  current GOP bootstraps without encoder reset
app/src/daemon/clip_buffer.{c,h}  encoded-stream spool sink; "clip" op extracts
                                  a [start,end] segment as codec-compatible MP4
                                  or VP8 WebM while recording continues;
                                  uses the first retained decoded frame's media
                                  PTS as its explicit zero,
                                  preserves stream-session epochs, crosses
                                  exactly compatible refreshes and rejects
                                  incompatible codec/config/geometry ranges
                                  without truncating time
app/src/daemon/report.{c,h}       test-report recorder + event logger; manifest
                                  owns dynamic video filename/codec/container/
                                  MIME metadata; events use host elapsed time
                                  from the first retained decoded frame while
                                  recorder/clip media use that frame's PTS;
                                  recorder stop is deferred until the event
                                  gate closes and the keeper clock freezes
app/src/daemon/plugin_event.{c,h} canonical one-event plugin report schema:
                                  complete inputs/assets, start/end/duration,
                                  result/status/exit/service/adoption metadata
app/src/daemon/addon.{c,h}        plugin add-ons (doc/addons.md); the Unified
                                  Plugin Protocol (metadata incl. service=true
                                  long-running add-ons, adaptive path/upload
                                  transport, SC_RESULT_FILE result) spans addon.c
                                  (register parse) + daemon.c (hello advertise,
                                  SC_ARG_*/SC_RESULT_FILE env, upload op, result
                                  read-back) + client.c (discover/group, --json,
                                  --daemon-host locality + upload) — keep in sync
app/src/daemon/registry.{c,h}   on-disk daemon registry
app/src/daemon/mirror.{c,h}     daemon mirror mode: `--client-port` with no op
                                opens the normal SDL window rendering the
                                daemon's subscribe_video stream (translates
                                daemon frames → the demuxer wire format so the
                                STOCK demuxer/decoder/screen are reused) and
                                forwards SDL input, camera torch/zoom and flex
                                display resize on a second connection
                                (doc/daemon.md §8.8)
app/src/third_party/jsmn.h      vendored JSON tokenizer (MIT)
doc/daemon.md                   daemon design + implementation notes
doc/fork.md                     this file
CLAUDE.md                       merge rules for AI-assisted work
app/scrcpy-auto.1, app/data/scrcpy-auto*.{png,desktop},
app/data/bash-completion/scrcpy-auto, app/data/zsh-completion/_scrcpy-auto
                                renamed data files (upstream edits their
                                old-named counterparts → rename-conflicts,
                                see §4)
```

### 3.2 Modified upstream files (conflict surface)

| File | Fork changes |
|---|---|
| `app/meson.build` | `executable('scrcpy-auto', ...)`; `libswscale` dependency; `src/control_exec.c`, `src/daemon/*.c`, `src/screencap.c` in `src`; renamed manpage/icons/completions/desktop `install_*` entries |
| `server/meson.build` | outputs `scrcpy-auto-server`, `install_dir: 'share/scrcpy-auto'`, devenv path |
| `app/src/server.c` | the three name defines (§2.1); everything else is upstream |
| `app/src/cli.c` | `OPT_SCREENCAP`, `OPT_CONTROL_CMD`, `OPT_DAEMON_PORT`, `OPT_CLIENT_PORT`, `OPT_DAEMON_STOP`, `OPT_DAEMON_STATUS`, `OPT_DAEMON_RECONNECT` (enum + option table + switch cases); `print_control_usage()`; `parse_daemon_reconnect()`; daemon/client validation block near the end of `parse_args_with_getopt()` (client mode **returns early**; a `--client-port` with **no operation** sets `opts->mirror` = mirror mode instead of erroring); `!opts->daemon_port` exemption in the "video disabled" auto-off check |
| `app/src/options.h` | fields: `screencap_filename`, `control_cmds[SC_MAX_CONTROL_CMDS]`, `control_cmd_count`, `daemon_port`, `client_port`, `mirror`, `daemon_stop`, `daemon_status`, `daemon_reconnect_none`, `daemon_reconnect_max`; internal `SC_RECORD_FORMAT_WEBM` for VP8 reports; `#define SC_MAX_CONTROL_CMDS 100` |
| `app/src/options.c` | defaults for the fields above |
| `app/src/events.h` | `SC_EVENT_SCREENCAP_COMPLETED`, `SC_EVENT_SCREENCAP_ERROR` |
| `app/src/scrcpy.c` | includes `control_exec.h`/`screencap.h`; screencap init/wiring/cleanup (search `screencap`); the `--control` execution block before `event_loop()` (calls `sc_control_exec_run(..., 1)`) |
| `app/src/main.c` | per-user `~/.scrcpy-auto` initialization before command dispatch; daemon/client routing between `sc_log_configure()` and `sc_main_thread_init()`; the client branch dispatches `opts->mirror` → `sc_mirror_run()` (wrapped in `sc_main_thread_init/destroy`) vs `sc_client_run()` |
| `app/src/user_config.{c,h}` | idempotent Unix startup initialization of the current user's private `~/.scrcpy-auto` directory |
| `app/src/util/net.{c,h}` | additive `net_local_port()` (getsockname helper) — used by mirror mode to find the ephemeral port of its loopback socket pairs |
| `app/src/trait/packet_source.h` | `SC_PACKET_SOURCE_MAX_SINKS` 2 → **4** (decoder + broadcaster + clip buffer + recorder) |
| `app/src/recorder.c` | internal WebM muxer mapping used by VP8 auto-test reports; explicit first-retained-frame media origin drops preroll and requires the first recorded packet to hit that origin exactly as a keyframe |
| `app/src/icon.h` | renamed icon filenames |
| `app/src/sdl_hints.c` | SDL app name `scrcpy-auto` |
| `app/scrcpy-windows.rc` | InternalName/OriginalFilename/ProductName |
| `run`, `server/build_without_gradle.sh` | renamed binary paths |

The `release/` packaging scripts and GitHub release workflow use the fork
artifact names (`scrcpy-auto`, `scrcpy-auto-server`, renamed icons/manpage and
archives). `install_release.sh` is still the upstream developer convenience
installer and is not part of the fork release archives.

## 4. Known conflict hotspots and how to resolve them

These are the exact places the scrcpy 4.0/4.1 merges conflicted, plus the ones
that will conflict on future merges. Default rule: **combine both sides**;
never resolve by taking only the upstream hunk.

1. **`app/src/cli.c`** — upstream constantly adds options.
   - `enum` block: keep fork `OPT_*` entries *and* new upstream entries.
   - Option table: keep the fork's `--screencap`, `--control`,
     `--client-port`, `--daemon-*` entries.
   - `switch` in `parse_args_with_getopt`: keep fork cases; watch for a
     dropped `break;` at hunk boundaries (happened in the 4.0 merge).
   - Validation section: the fork block must stay **before** upstream's
     option post-processing, and client mode's early `return true` must
     remain after all fork validations.
2. **`app/src/options.h` / `options.c`** — keep fork fields/defaults at the
   end of the struct; append upstream's new ones alongside.
3. **`app/src/events.h`** — keep both sides' event enum entries.
4. **`app/meson.build`** — keep `libswscale` (screencap needs it even if
   upstream doesn't), the fork source files, the `scrcpy-auto` names.
5. **Renamed data files** — upstream edits `app/data/scrcpy.desktop` etc.;
   git usually follows the rename, but verify the fork names and contents
   (`Exec=scrcpy-auto`, completion command names) survive.
6. **`app/src/server.c`** — upstream edits near the top of the file;
   re-check the three renamed defines after every merge:
   `grep -n 'scrcpy-auto-server\|share/scrcpy-auto' app/src/server.c`
   must show all three.
7. **`app/src/trait/packet_source.h`** — if upstream changes
   `SC_PACKET_SOURCE_MAX_SINKS`, the fork value must remain ≥ 4.

## 5. Semantic coupling: upstream changes that break the fork WITHOUT conflicts

Fork-owned files call upstream-internal APIs. A merge can be textually clean
and still not compile — or worse, compile and misbehave. After every merge,
check these:

| Fork consumer | Upstream API it depends on | Past precedent |
|---|---|---|
| `screencap.c` | `sc_packet_sink_ops.open()` signature | 4.0 added a `const struct sc_stream_session *` param; the fork sink had to be adapted |
| `daemon/frame_keeper.c` | `sc_frame_sink_ops` signatures (`trait/frame_sink.h`) | same class of risk |
| `daemon/daemon.c` | `struct sc_server_params` — **duplicated** from `scrcpy.c`'s initializer; new upstream fields must be mirrored (or the daemon silently ignores the new option) | two audio fields were missed once; 4.1 added `ignore_video_encoder_constraints` |
| `daemon/daemon.c` | `sc_server_*`, `sc_demuxer_*`, `sc_decoder_init`, `sc_controller_*` signatures and callback contracts | — |
| `control_exec.c`, `daemon/daemon.c` | `struct sc_control_msg` layout (`inject_touch_event`, `set_clipboard`), `SC_CONTROL_MSG_TYPE_RESET_VIDEO` | — |
| `daemon/mirror.c` | the **serialized** control-message wire layout (`sc_control_msg_serialize` in `control_msg.c`): the control adapter parses the controller's bytes back into daemon requests, so per-type field offsets/sizes and the `enum sc_control_msg_type` order are hard-coded. Re-verify every case against `control_msg.c`, including consuming unsupported messages at their exact encoded length; 4.1 added `SCAN_FILE`, and camera/flex-display messages need explicit forwarding. A wrong length desynchronizes the stream. | 4.1 added `SCAN_FILE` |
| `daemon/broadcaster.c`, `daemon/clip_buffer.c` | packet-sink stream-session/config semantics: codec config is optional for VP8/VP9, and every new session may change geometry | 4.1 added VP8/VP9 |
| `daemon/protocol.c`, `daemon/client.c` | `net_*` API (`util/net.h`) | — |
| `screencap.c`, `frame_keeper.c` | FFmpeg API level used by upstream | FFmpeg 8 migration was absorbed in the 4.0 merge |

The most reliable detector is a full-tree compile (see §6): all of these
break loudly at compile time **except** the `sc_server_params` duplication,
which must be diffed by hand:

```
# compare field coverage of the two initializers
grep -o '\.\w*' app/src/scrcpy.c    | sort -u > /tmp/a   # inside its params block
grep -o '\.\w*' app/src/daemon/daemon.c | sort -u > /tmp/b
```

(or simply diff the two `struct sc_server_params params = {` blocks.)

## 6. Post-merge verification checklist

1. **No conflict markers**: `grep -rn '<<<<<<<\|>>>>>>>' app/ server/ doc/`
2. **Meson parses**: `meson format app/meson.build > /dev/null` (any
   meson ≥ 1.5; a pip-free tarball from GitHub works standalone).
3. **Full compile, warning-free.** With a normal toolchain:
   `meson setup x && ninja -Cx`. On a machine without one, the standalone
   recipe that has worked here:
   - download `zig` (self-contained clang), the SDL3 source tarball and the
     FFmpeg source tarball;
   - write a stub `config.h` (mirror the `conf.set` values in
     `app/meson.build`) and a 3-line `libavutil/avconfig.h` stub;
   - compile every non-Windows, non-USB, non-V4L2 file under `app/src` with
     `zig cc -c -std=c11 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
     -D_XOPEN_SOURCE=700 -I<stubs> -Isrc -I<SDL3>/include -I<ffmpeg>`.
   The tree must compile with **zero warnings** — that is the state every
   fork commit has maintained.
4. **Fork invariants** (cheap greps):
   - `grep -n "executable('scrcpy-auto'" app/meson.build`
   - `grep -n 'scrcpy-auto-server' app/src/server.c server/meson.build`
   - `grep -c 'OPT_SCREENCAP\|OPT_CONTROL_CMD\|OPT_DAEMON_PORT\|OPT_CLIENT_PORT' app/src/cli.c` (expect ≥ 8)
   - `grep -n 'SC_PACKET_SOURCE_MAX_SINKS 4' app/src/trait/packet_source.h`
5. **Functional smoke test** (needs a device):
   - `scrcpy-auto -s X --no-window --screencap /tmp/s.png`
   - `scrcpy-auto -s X --control "click 100 100"`
   - normal mode: exercise VP8/VP9 and any other upstream 4.1 feature touched
     by the merge on a device that advertises the corresponding encoder;
   - daemon: start with `--daemon-port 27400 --no-window`, then
     `--client-port 27400 --daemon-status`, `--screencap`, `--control`,
     unplug/replug the device (reconnection), `--daemon-stop`.
   - daemon codecs: for every encoder the device supports, verify
     `subscribe_video` for h264/h265/av1/vp8/vp9; VP8 must not wait for a
     config packet and its clip/report manifest must identify WebM, while the
     other codecs identify MP4.

## 7. Version history of the fork

| Commit | What |
|---|---|
| `4485bde4` | original `--screencap` / `--control` feature ("save code") |
| `77a3d25e` | merge of upstream 4.0 (SDL3, FFmpeg 8) with conflict resolutions |
| `12f0925c` | rename to scrcpy-auto / scrcpy-auto-server |
| `b28181e3` | daemon design document (`doc/daemon.md`) |
| `8c373c62` | daemon mode implementation |
| `e66c1cd6` | code-review fixes for daemon mode |
| `3e4e7aed` | merge of upstream 4.1 into `feat/control` |
