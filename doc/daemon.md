# Persistent daemon mode — design document

Status: **v1 implemented** (phases 0–2 plus reconnection; see §15 — remaining:
`--client-serial` registry resolution, protocol/registry unit tests in the
meson test harness)
Target branch: `feat/control`
Applies to: scrcpy-auto (fork of scrcpy 4.0)

Implementation notes (deviations from the original proposal):
 - Instead of refactoring `scrcpy()` into a shared `sc_session` module
   (§15 phase 0), the daemon owns a dedicated lean session bring-up in
   `app/src/daemon/daemon.c` (server + video demuxer + decoder + frame
   keeper + controller only). One-shot mode is untouched, which removes the
   riskiest refactor. Only the `--control` executors moved, to
   `app/src/control_exec.c`.
 - The frame keeper does not wrap `sc_frame_buffer` (whose frames are
   consume-once); it keeps its own re-readable latest frame
   (`app/src/daemon/frame_keeper.c`).
 - Client exit code 2 reuses `SCRCPY_EXIT_DISCONNECTED`.
 - The clipboard-injection mutex serializes whole `control` requests that
   contain an `input` step (coarser than §9.5, same guarantee).
 - Touch pointer-id ranges are derived from the connection slot index
   (disjoint by construction) instead of a wrapping allocator.
 - Control step durations (`click`/`swipe` duration, `sleep`) are capped at
   60 s per step so a request cannot block daemon teardown for hours
   (§14 edge case).

---

## 1. Problem statement

Today, every automation invocation pays the full session setup cost. A command
such as:

```bash
scrcpy-auto -s DEVICEID_XX1 --no-window --screencap out.png
scrcpy-auto -s DEVICEID_XX1 --no-window --control "click 500 800"
```

runs the complete startup sequence implemented in `run_server()`
(`app/src/server.c:957`):

1. `adb start-server` (no-op if running, still a process spawn)
2. device selection (`adb devices` parse)
3. `adb push` of `scrcpy-auto-server` to `/data/local/tmp/scrcpy-auto-server.jar`
4. tunnel setup (`adb reverse`, fallback `adb forward`)
5. `app_process` launch of the Java server on the device (ART VM spin-up,
   encoder + input manager initialization)
6. up to 3 socket accepts (video/audio/control) + device metadata handshake
   (`sc_server_connect_to()`, `app/src/server.c:605`)

then tears everything down at exit (socket shutdown, watchdog kill of the
device process, `app/src/server.c:1140`). In practice each invocation costs
roughly 1.5–3 s, dominated by steps 3–5 — none of which depend on the actual
request being made.

**Goal**: start that session once, keep it alive in a background *daemon*
process, and let subsequent thin *client* invocations execute operations
(screenshot, control commands, future operations) over a local IPC channel in
tens of milliseconds.

Proposed UX (as requested):

```bash
# one-time: start a persistent daemon for one device
scrcpy-auto -s DEVICEID_XX1 --daemon-port 27400 --no-window &

# afterwards: fast operations, no serial needed
scrcpy-auto --client-port 27400 --screencap out.png
scrcpy-auto --client-port 27400 --control "click 500 800"
scrcpy-auto --client-port 27400 --control "swipe 100 100 900 900 300" --control "click 50 50"
```

> Note on naming: the request used `--no-display`; that was the scrcpy 1.x
> flag, renamed upstream to `--no-window` (`OPT_NO_WINDOW`, `app/src/cli.c`).
> This document uses `--no-window`. Daemon mode requires it in v1 (§6.2).

---

## 2. Feasibility analysis

### 2.1 What the codebase already provides

The conclusion up front: **the architecture is feasible**, and fits the
existing design unusually well. The load-bearing facts:

| Requirement | Existing mechanism |
|---|---|
| Long-lived device session | The client↔device session already lives until `sc_server_stop()`; `run_server()` just parks in a condvar wait after connect (`app/src/server.c:1119`). Nothing times out on its own. |
| Injecting input from an arbitrary thread | `sc_controller_push_msg()` (`app/src/controller.h:68`) is a mutex-protected queue drained by the controller thread. The `feat/control` command executor already pushes from worker threads. |
| Keeping "the latest frame" for screenshots | `struct sc_frame_buffer` (`app/src/frame_buffer.h:21`) already implements exactly this: it retains the most recent decoded `AVFrame` under a mutex. The daemon adds a *frame keeper* sink reusing it. |
| Attaching extra consumers to the video stream | Packet sources and frame sources are multi-sink by design (`SC_PACKET_SOURCE_MAX_SINKS` = 3, `SC_FRAME_SOURCE_MAX_SINKS` = 2). |
| Forcing a fresh keyframe on demand | `SC_CONTROL_MSG_TYPE_RESET_VIDEO` (`app/src/control_msg.h:45`) makes the device encoder restart and emit an IDR frame — useful for freshness guarantees (§9.4). |
| Cross-platform sockets | `app/src/util/net.c` already wraps TCP for Linux/macOS/Windows; the IPC listener reuses it. |
| Interruptible blocking calls | `sc_intr` / `net_interrupt` allow clean shutdown of every blocking call. |
| PNG encoding of a frame | `sc_screencap_save_frame_as_png()` (`app/src/screencap.c:18`) — swscale → RGB24 → PNG via FFmpeg. Reused nearly as-is. |

### 2.2 What is missing

1. **Session re-entrancy.** `scrcpy()` (`app/src/scrcpy.c`) is written as a
   one-shot function: a `static struct scrcpy` instance, a `goto end` cleanup
   ladder, and `main()` calls it exactly once (`app/src/main.c:93`). Upstream
   4.0's "disconnected" feature does *not* reconnect — it shows an icon until
   a timeout and exits (`sc_screen_handle_disconnection()`,
   `app/src/screen.c:1198`). A daemon must be able to tear down and rebuild
   the device session *within one process* (§10). This requires extracting
   the session setup/teardown into a re-invocable module — the largest single
   refactor of this project.

2. **Headless decoding.** The video decoder is only instantiated when
   `video_playback || v4l2` (`app/src/scrcpy.c:1066`); with `--no-window`
   nothing decodes. The daemon must force a decoder and attach its frame
   keeper (§9.2).

3. **Why we cannot just reuse the current `--screencap` sink per request:**
   the existing implementation waits for a *config packet + keyframe*, which
   the device only sends at stream start. On an already-running stream, a
   newly attached packet sink would wait indefinitely (H.264 P-frames are
   undecodable without the preceding reference chain). Persistent screenshots
   therefore require either continuous decode (chosen, §9.2) or per-request
   `RESET_VIDEO` (rejected as primary, §9.4).

4. **The IPC layer itself** — listener, protocol, request dispatch, thin
   client mode (§8).

5. **Daemon discovery/registry**, lifecycle management, reconnection
   supervision (§6, §7, §10).

### 2.3 Verdict

Feasible with moderate effort. No changes to the Android-side server are
required for v1 (the daemon speaks the existing scrcpy protocol on one side
and a new local protocol on the other). Estimated effort: ~2,000–2,500 new
LOC client-side plus a ~400-line refactor of `scrcpy.c` (§15).

---

## 3. Goals and non-goals

**Goals (v1)**

- One daemon process per device, listening on a localhost TCP port.
- Thin client mode executing: screenshot, control commands (the existing
  `--control` mini-language), status query, daemon shutdown.
- Millisecond-scale request latency once the session is up.
- Automatic reconnection to the device with backoff.
- Multi-device via multiple daemons + a small on-disk registry.
- Exact backward compatibility when the new flags are absent.

**Non-goals (v1)** — listed as future work in §16:

- Streaming video/audio to clients over IPC.
- A daemon with a visible mirroring window (`--daemon-port` requires
  `--no-window` in v1).
- Remote (non-localhost) clients, authentication, encryption.
- One daemon managing several devices (§11 explains why).
- OTG/AOA (USB HID) mode in the daemon.

---

## 4. Architecture overview

```
                       ┌────────────────────────────── PC ──────────────────────────────┐
                       │                                                                 │
 scrcpy-auto           │   scrcpy-auto --daemon-port 27400 -s XX1 --no-window            │
 --client-port 27400   │  ┌───────────────────────────────────────────────┐             │
┌─────────────┐  TCP   │  │                DAEMON PROCESS                  │   adb+TCP   │   ┌─ device ─┐
│ thin client ├────────┼──► IPC listener (127.0.0.1:27400)                 │             │   │          │
└─────────────┘ NDJSON │  │   │ conn thread(s): parse → dispatch           │             │   │ app_     │
                       │  │   ▼                                            │             │   │ process  │
┌─────────────┐        │  │ request executor ──► sc_controller_push_msg ───┼─────────────┼───► scrcpy-  │
│ thin client ├────────┼──►   │                    (control socket)        │             │   │ auto-    │
└─────────────┘        │  │   └─► frame keeper (latest AVFrame) ◄─ decoder ◄─ demuxer ◄──┼───┤ server   │
                       │  │                                     (video socket)           │   │          │
                       │  │ session supervisor: (re)start/stop device session, backoff   │   └──────────┘
                       │  └───────────────────────────────────────────────┘             │
                       │        registry file: $XDG_RUNTIME_DIR/scrcpy-auto/27400.json   │
                       └─────────────────────────────────────────────────────────────────┘
```

Three roles:

- **Device server** (`scrcpy-auto-server`, unchanged): pushed and launched by
  the daemon exactly as today.
- **Daemon** (new mode of the `scrcpy-auto` binary): owns the device session
  (demuxer/decoder/controller threads), keeps the latest decoded frame,
  exposes a request/response IPC endpoint, supervises reconnection.
- **Thin client** (new mode of the same binary): parses a restricted CLI
  subset, connects to the daemon, sends one or more requests, prints/writes
  results, exits. Never touches adb.

---

## 5. CLI surface

### 5.1 New options

| Option | Mode | Argument | Meaning |
|---|---|---|---|
| `--daemon-port PORT` | daemon | 1–65535 | Run as daemon; listen on 127.0.0.1:PORT. |
| `--client-port PORT` | client | 1–65535 | Run as thin client of the daemon on 127.0.0.1:PORT. |
| `--client-serial SERIAL` | client | serial | Alternative to `--client-port`: resolve the port via the registry (§7.2). |
| `--daemon-reconnect POLICY` | daemon | `auto[:MAX]` \| `none` | Device-loss policy; default `auto` (unbounded retries, §10). |
| `--daemon-stop` | client | — | Ask the daemon to shut down gracefully. |
| `--daemon-status` | client | — | Print daemon/session state as JSON to stdout. |

Implementation: new `OPT_*` enum entries and `sc_option` rows in
`app/src/cli.c`, new fields in `struct scrcpy_options` (`app/src/options.h`):
`uint16_t daemon_port; uint16_t client_port; const char *client_serial;
enum sc_daemon_reconnect daemon_reconnect; bool daemon_stop; bool daemon_status;`.

### 5.2 Option compatibility matrix

Validated at the end of `parse_args()`; violations are fatal CLI errors.

- `--daemon-port` **excludes** `--client-port`, `--client-serial`,
  `--screencap`, `--control`, `--record`, `--v4l2-sink`, `--otg`,
  `--list-*`, `--start-app`, `--tcpip=+addr` is allowed (any device-selection
  option is allowed — the daemon owns device selection).
- `--daemon-port` **requires** `--no-window` (v1; see §6.2) and `--control`
  capability enabled (i.e. it is an error to combine with `-n/--no-control`
  unless screenshots-only operation is intended — allowed but logged).
- `--client-port`/`--client-serial` **exclude** every option that concerns
  session creation: `-s`, `-d`, `-e`, `--tcpip`, port/tunnel options,
  codec/bit-rate options, window options, `--record`, `--otg`, …
  Allowed with: `--screencap FILE`, `--control CMD` (repeatable),
  `--daemon-stop`, `--daemon-status`, `--time-limit` (as request timeout).
- `--daemon-stop`/`--daemon-status` require client mode.

If none of the new flags are present, behavior is bit-for-bit today's (§13).

### 5.3 Exit codes (client mode)

Reuses `enum scrcpy_exit_code`:

- `0` — request(s) succeeded.
- `1` — request failed on the daemon side (device disconnected, command
  invalid, …). The error is printed to stderr.
- `2` — could not reach/handshake the daemon (connection refused, protocol
  mismatch, stale registry entry).

---

## 6. Daemon lifecycle

### 6.1 Startup sequence

1. Parse CLI; enter daemon mode.
2. Bind + listen on `127.0.0.1:PORT` **before** any adb work. If the bind
   fails (`EADDRINUSE`), exit immediately with a clear error — this is the
   "already running?" check, race-free because the OS arbitrates the port.
3. Write the registry entry (§7.1) marked `"state": "starting"`.
4. Start the device session (existing `sc_server_start()` path) with
   `video=true, audio=false (default), control=true, video_playback=off,
   window=off`, plus the daemon frame keeper (§9.2).
5. On `on_connected`: mark registry `"ready"`, log
   `Daemon ready on 127.0.0.1:PORT (device XXX)`.
6. Serve requests until stopped.

The daemon runs in the **foreground** (user backgrounds it with `&`, systemd,
Task Scheduler…). A self-daemonizing `--daemon-detach` (double-fork) is
deliberately deferred: it is POSIX-only, complicates logging, and every modern
supervisor prefers foreground children. Logs go to stderr as today.

### 6.2 Why `--no-window` is required in v1

The SDL window path pulls in: render thread assumptions on the main thread,
`sc_screen` as a frame sink consuming one of the two frame-source slots, and
the *blocking* disconnect UI (`sc_screen_handle_disconnection`) which would
stall the supervisor loop. Excluding the window keeps the daemon's main
thread free to run the (much simpler) event loop and makes reconnection
tractable. Lifting this restriction is future work (§16).

Even with `--no-window`, SDL is still initialized with events only —
`sc_push_event()` / the main event loop keep working, which the daemon reuses
for cross-thread signalling (§9.1).

### 6.3 Shutdown paths

| Trigger | Behavior |
|---|---|
| SIGINT/SIGTERM (Ctrl+C) | Graceful: stop IPC listener, fail in-flight requests with `E_SHUTDOWN`, stop session (`sc_server_stop()` — sockets shut down, device process reaped with the existing 1 s watchdog, `app/src/server.c:1140`), remove registry entry, exit 0. |
| Client `shutdown` request | Same as above, after acking the request. |
| Device permanently lost and `--daemon-reconnect none` (or retries exhausted) | Same, exit 1. |
| SIGKILL / crash | Registry entry goes stale (handled by liveness checks, §7.3). Device-side `app_process` exits on its own when its sockets die (same property that makes plain scrcpy kills safe today). |

### 6.4 Idle policy

None in v1: the session stays up 24/7. Note for users: a mirrored device
keeps its encoder pipeline active; combine with `--screen-off-timeout`,
`--turn-screen-off`, `--stay-awake`, and a modest `--max-fps` (§9.2) to bound
device-side cost.

---

## 7. Client discovery

### 7.1 Registry

Directory (created `0700`):

- Linux: `$XDG_RUNTIME_DIR/scrcpy-auto/` (fallback `/tmp/scrcpy-auto-$UID/`)
- macOS: `~/Library/Application Support/scrcpy-auto/run/`
- Windows: `%LOCALAPPDATA%\scrcpy-auto\run\`

One JSON file per daemon, named `<port>.json`, written atomically
(write temp + rename):

```json
{
  "registry_version": 1,
  "port": 27400,
  "pid": 12345,
  "serial": "DEVICEID_XX1",
  "device_name": "Pixel 8",
  "state": "ready",
  "started_at": "2026-07-03T10:00:00Z",
  "protocol": 1
}
```

### 7.2 Resolution rules (client)

1. `--client-port PORT` → connect to `127.0.0.1:PORT` directly. The registry
   is *not* consulted; the port is the source of truth (the user's requested
   UX works even if the registry dir is wiped).
2. `--client-serial SERIAL` → scan registry, pick the live entry with that
   serial; error if none/ambiguous.
3. Neither given (future convenience, still specified): if the registry holds
   exactly one **live** entry, use it; if zero → error `no scrcpy-auto daemon
   running`; if several → error listing them (`use --client-port or
   --client-serial`).

### 7.3 Staleness

An entry is live iff (a) a TCP connect to the port succeeds and (b) the
`hello` handshake (§8.2) returns matching `pid`/`serial`. Clients and daemons
delete entries that fail (a) at scan time. PID-liveness checks are only a
fast-path hint (PIDs recycle; the handshake is authoritative).

---

## 8. IPC protocol

### 8.1 Transport and framing

TCP on `127.0.0.1` only (IPv4 loopback; simplest portable choice — Windows
has no usable unix sockets across all supported versions, and `net.c` already
abstracts TCP).

Every message (both directions) is one **frame**:

```
uint32  big-endian  length of the JSON document in bytes
bytes   UTF-8 JSON document (single object)
bytes   optional binary payload, exactly header.payload_len bytes (0 if absent)
```

JSON keeps the protocol debuggable and extensible; the binary payload channel
keeps PNG bytes out of JSON. Frame size hard cap: 64 MiB (sanity).

The parser dependency is deliberately tiny: vendor `jsmn` (single header,
MIT — license-compatible) for parsing; generation is `sc_strbuf`-style manual
printf (the schema is small and flat). No new external build dependency.

### 8.2 Handshake

On connect, the **daemon speaks first** (avoids a client round trip):

```json
{"event":"hello","protocol":1,"app":"scrcpy-auto","version":"4.0",
 "pid":12345,"serial":"DEVICEID_XX1","device_name":"Pixel 8",
 "state":"ready"}
```

The client checks `protocol`. Mismatch → client aborts with exit code 2 and a
message telling the user to update one side. `protocol` is bumped only on
incompatible changes; additive fields are allowed without a bump.

### 8.3 Requests

Client → daemon; `id` is a client-chosen integer echoed in the response,
allowing pipelining (v1 clients send sequentially, the field future-proofs).

| `op` | Params | Success response extras |
|---|---|---|
| `ping` | — | — |
| `status` | — | `state`, `device` object, `uptime_ms`, `last_frame_age_ms` |
| `screencap` | `format` (`"png"`, only value in v1), `max_age_ms` (optional, §9.4) | `width`, `height`, `payload_len` + PNG bytes as payload |
| `control` | `cmds`: array of strings — each string one `--control` argument, same mini-language and same parallel/`&&` semantics as the CLI (`sc_execute_control_cmds`, `app/src/scrcpy.c:764`) | — (returns after execution completes, like the CLI does) |
| `shutdown` | — | — (daemon exits after responding) |

Response envelope:

```json
{"id":7,"ok":true, ...op-specific fields..., "payload_len":0}
{"id":7,"ok":false,"error":{"code":"E_DEVICE_DISCONNECTED","message":"..."}}
```

### 8.4 Error codes

| Code | Meaning | Client exit code |
|---|---|---|
| `E_BAD_REQUEST` | malformed JSON/unknown op/invalid params | 1 |
| `E_NOT_READY` | session not (yet) connected; `state` included | 1 |
| `E_DEVICE_DISCONNECTED` | lost mid-request | 1 |
| `E_TIMEOUT` | op exceeded its deadline (e.g. no frame, §9.3) | 1 |
| `E_BUSY` | conflicting exclusive op in progress (§14.4) | 1 |
| `E_SHUTDOWN` | daemon stopping | 1 |
| `E_INTERNAL` | anything else; details in message | 1 |

### 8.5 Example exchange

```
client → {"id":1,"op":"screencap","format":"png"}
daemon → {"id":1,"ok":true,"width":1080,"height":2400,"payload_len":183220}
         <183220 bytes of PNG>
client writes out.png locally, exits 0
```

The PNG is returned as payload and written **by the client** in its own
working directory — keeping `scrcpy-auto --client-port 27400 --screencap
shots/a.png` semantics identical to the one-shot mode (relative paths resolve
in the caller's cwd, not the daemon's).

### 8.6 CLI ↔ protocol mapping (client mode)

- `--screencap FILE` → one `screencap` request; payload written to FILE.
- `--control CMD` (×N) → one `control` request with `cmds:[...]` preserving
  the CLI's multi-arg semantics.
- `--daemon-status` → `status`, pretty-printed to stdout.
- `--daemon-stop` → `shutdown`.
- Combinations execute in a fixed order: control → screencap → status → stop
  (mirrors "act, then observe", and matches one-shot mode where `--control`
  runs before exit).

---

## 9. Daemon internals

### 9.1 Threading model

Reuses the existing pattern (dedicated threads + `sc_push_event` to the main
loop):

- **main thread**: SDL events-only loop (as `--no-window` runs today);
  handles lifecycle events (`SC_EVENT_DAEMON_*`, session ended, signals) and
  runs the session supervisor state machine (§10).
- **existing session threads**: server thread, demuxer(s), decoder, controller
  — untouched.
- **IPC accept thread**: `accept()` loop; spawns one **connection thread**
  per client (clients are short-lived; concurrency is small; a thread per
  connection is the simplest correct model and matches the codebase style —
  no event-loop reactor needed).
- Connection threads execute requests directly: `sc_controller_push_msg` is
  already thread-safe; frame keeper access is mutex-guarded; PNG encoding is
  CPU work fine to do on the connection thread.

### 9.2 Frame keeper (screenshots)

New module `app/src/daemon/frame_keeper.c` implementing the
`sc_frame_sink` trait:

- wraps an `sc_frame_buffer` (latest-frame semantics, exactly its upstream
  purpose) plus `last_frame_tick`, a `sc_cond` broadcast on every push, and
  the frame's width/height.
- The daemon forces `needs_video_decoder = true` and attaches the keeper to
  the decoder's frame source (a free slot: with no window and no v4l2, both
  `SC_FRAME_SOURCE_MAX_SINKS` slots are unused).
- `screencap` request: lock, take a ref of the newest frame
  (`sc_frame_buffer_consume`-style swap keeps decode unblocked), unlock,
  convert + encode via the existing `sc_screencap_save_frame_as_png()`
  refactored into `sc_frame_to_png(AVFrame *, uint8_t **out, size_t *len)`
  (the current file-writing wrapper stays for one-shot mode).

CPU note: continuous decode is unavoidable for on-demand screenshots (H.264
frames reference their predecessors; you cannot decode "only when asked").
Cost control belongs on the encode side: daemon docs recommend `--max-fps 10`
and reduced `-m` size for automation daemons; both flow through untouched.

An important encoder property that makes this cheap in practice: Android
MediaCodec emits frames **only when the screen content changes**. An idle
device produces no packets, so an idle daemon consumes ~0 CPU. Corollary: the
"latest frame" can be arbitrarily old *and still pixel-correct* — staleness
of the frame is not staleness of the screenshot.

### 9.3 First frame

Immediately after (re)connection there may be no decoded frame yet (the
device sends config + first IDR within ~100 ms of encoder start).
`screencap` therefore waits on the keeper's condvar with a deadline
(default 2 s, `E_TIMEOUT` after) instead of failing instantly.

### 9.4 Freshness (`max_age_ms`) and `RESET_VIDEO`

If a request sets `max_age_ms` and `now - last_frame_tick` exceeds it, the
daemon sends `SC_CONTROL_MSG_TYPE_RESET_VIDEO` (forces the encoder to restart
→ fresh IDR) and waits for the next frame push (same deadline machinery).
Default is *no* freshness constraint, because of the corollary in §9.2 — the
latest frame already equals the current screen except during the tiny window
of an in-flight change. `RESET_VIDEO` is the escape hatch, not the norm — it
restarts the encoder, costing ~100–300 ms.

### 9.5 Control command execution

The `feat/control` executor is reused with two changes:

- `sc_execute_control_cmds()` and helpers move from `app/src/scrcpy.c` into
  `app/src/control_exec.{c,h}` (they are already self-contained: parser,
  touch interpolation threads, clipboard-paste text injection).
- **Pointer-id allocation**: one-shot mode uses fixed ids 1..N. With
  concurrent clients, two requests would collide on pointer id 1 and corrupt
  each other's gestures. The daemon allocates each `control` request a
  disjoint id range from an atomic counter (wrapping within a sane band,
  e.g. ids 1–999; POINTER_ID_MOUSE/VIRTUAL reserved values stay clear).
- **Text-input serialization**: `input` uses SET_CLIPBOARD + paste; two
  concurrent pastes race on the device clipboard. A daemon-global mutex
  serializes the clipboard-dependent step only (touch streams stay parallel).

---

## 10. Error handling and reconnection

### 10.1 Session supervisor state machine (daemon main thread)

```
            ┌────────────┐ connect ok ┌─────────┐ device lost ┌──────────────┐
  start ───►│ CONNECTING ├───────────►│  READY  ├────────────►│ RECONNECTING │
            └─────┬──────┘            └────┬────┘             └───┬──────────┘
                  │ fatal error            │ stop/shutdown        │ backoff, then
                  ▼                        ▼                      │ re-enter CONNECTING
              exit(1)                  STOPPING ◄─────────────────┘ (or exit(1) if
                                        exit(0)                     policy exhausted)
```

- *Device lost* is detected exactly as today: demuxer EOF/error events and
  `on_disconnected` (`app/src/scrcpy.c:278`). The supervisor tears the
  session down with the existing cleanup path, then re-runs the (refactored)
  session bring-up.
- **Backoff**: 1 s, 2 s, 4 s … capped at 30 s; reset on success.
  `--daemon-reconnect none` → exit instead; `auto:MAX` → give up after MAX
  consecutive failures.
- Distinguishing failures: "device not in `adb devices`" (device unplugged —
  keep retrying quietly) vs "adb server unreachable" (retry also covers it —
  `run_server` re-runs `adb start-server` each attempt) vs "session refused"
  (e.g. server jar version mismatch — fatal, exit 1, message).

### 10.2 Client-visible behavior

- Requests arriving in `CONNECTING`/`RECONNECTING` fail fast with
  `E_NOT_READY` + current state; clients print it and exit 1. (A blocking
  `wait_ready` op is future work; automation scripts can poll `status`.)
- Requests in flight when the device drops get `E_DEVICE_DISCONNECTED`.
  In-progress touch gestures are simply lost (matching what a cable pull does
  to one-shot mode).
- The IPC listener stays up during reconnection — clients get structured
  errors rather than connection refused, and `status` works throughout.

### 10.3 Client-side robustness

The thin client is deliberately dumb: one connect attempt, one
request/response cycle, hard 10 s I/O timeout (configurable via
`--time-limit`), no retries (retrying a `control` request could double-tap;
idempotency belongs to the caller's script).

### 10.4 Registry hygiene

Covered in §7.3; additionally the daemon refreshes its entry on every state
transition, so `status`-via-registry is never more than one transition stale.

---

## 11. Multi-device support

**Model: one daemon per device, one port per daemon.**

```bash
scrcpy-auto -s DEVICE_A --daemon-port 27400 --no-window &
scrcpy-auto -s DEVICE_B --daemon-port 27401 --no-window &

scrcpy-auto --client-port 27400 --control "click 100 100"   # device A
scrcpy-auto --client-serial DEVICE_B --screencap b.png       # device B via registry
```

Rationale for rejecting a single multi-device daemon:

- The device session is inherently per-device anyway (scid, tunnel, 3
  sockets, threads); a multi-device daemon is just N sessions in one process
  — all complexity (per-device state routing, partial failure semantics,
  option sets per device) and no capability gain.
- Process-per-device gives free fault isolation (one device's crash cannot
  take down another's session) and lets each daemon use different options
  (bit-rate, max-fps) naturally.
- The registry already provides the "fleet view" (`--daemon-status` per
  entry; a future `--daemon-list` is trivial).

Two daemons for the *same* device are technically possible (distinct scids —
same reason two scrcpy instances work today) but wasteful; the daemon logs a
warning if the registry already holds a live entry for its serial.

---

## 12. Security considerations

- The listener binds `127.0.0.1` **only**; never `0.0.0.0`. Remote access is
  out of scope (users can SSH-tunnel deliberately).
- Trust boundary = local user account. Anyone who can connect to localhost
  can control the device — identical to the trust level of `adb` itself on
  the same machine. Registry files are `0600` in a `0700` dir.
- No secrets in the protocol; no shell execution server-side (the `control`
  mini-language maps to fixed control messages, never to `sh`).
- Frame cap (64 MiB) and request-size limits guard against memory abuse by
  local processes.

---

## 13. Backward compatibility

- **No new flags → no behavior change.** Daemon/client code paths are
  reached only via `--daemon-port`/`--client-port`/`--client-serial`.
- One-shot `--screencap`/`--control` keep working exactly as on
  `feat/control` today (same code, post-refactor).
- The device server binary and protocol are untouched — the daemon is purely
  a client-side arrangement. `scrcpy-auto-server` remains compatible with
  both modes, and official scrcpy on the same machine/device remains
  unaffected (separate names, §rename commit `12f0925c`).
- The CLI validation matrix (§5.2) turns every ambiguous combination into an
  explicit error rather than silent reinterpretation.

---

## 14. Edge cases and implementation challenges

1. **`scrcpy()` re-entrancy refactor** (§2.2.1) — the critical path. The
   `static struct scrcpy` + `goto end` ladder must become
   `sc_session_start()/stop()/join()` with an explicit owned-resources
   struct. Mitigation: mechanical extraction first (one-shot mode still
   calls it once), daemon loop added only after that lands and one-shot
   behavior is verified unchanged.
2. **SDL main-thread coupling** — `sc_push_event`, `sc_main_thread_*`
   runnables and the timeout module assume one SDL event loop. The daemon
   keeps that loop as its supervisor loop, so the assumption holds; but
   session *re*-init must not re-run one-time SDL init (`sc_main_thread_init`
   etc. move out of the per-session path).
3. **Frame-sink slot budget** — `SC_FRAME_SOURCE_MAX_SINKS` is 2; daemon
   uses 1. If the window restriction is ever lifted, bump the constant
   (trivial, array-sized).
4. **Concurrent `control` requests** — pointer-id ranges + clipboard mutex
   (§9.5). Additionally the *screen-size denominator*: injected coordinates
   are normalized against `UINT16_MAX` pseudo-size by the `feat/control`
   executor, which the server maps to the current stream size — rotation
   mid-gesture still lands wrong (already true today; documented, not
   solved).
5. **Device rotation / resolution change** — the demuxer handles stream
   reconfig; the keeper just swaps in differently-sized frames.
   `screencap` reports per-frame `width`/`height`, so clients always learn
   the real geometry.
6. **No-video daemons** — `--no-video` + daemon is allowed (control-only
   automation); `screencap` then answers `E_BAD_REQUEST` ("daemon started
   with --no-video"). Validation warns at startup.
7. **Sleep/hibernate of the PC** — sockets die on resume; this is just
   "device lost" → RECONNECTING. Backoff makes wake storms harmless.
8. **adb server restarts** (e.g. Android Studio grabbing the port) — same
   path; each retry re-runs `adb start-server`.
9. **Port collisions** — bind-first startup (§6.1) makes the failure mode
   immediate and obvious; the registry is advisory only, never arbitrates.
10. **Windows specifics** — no fork/daemonize (§6.1 foreground-only design
    sidesteps it); `SO_REUSEADDR` semantics differ (Windows allows port
    hijack — mitigated by loopback bind + handshake pid check); console
    signal handling uses the existing scrcpy Ctrl+C machinery.
11. **Registry dir on headless CI** — `$XDG_RUNTIME_DIR` may be absent;
    fallback path defined in §7.1; registry failure is a warning, not fatal
    (explicit `--client-port` keeps working — §7.2.1).
12. **Large screenshots** — 4K tablets ≈ 30–60 MiB raw, a few MiB as PNG;
    within the 64 MiB frame cap. Encoding on the connection thread keeps the
    decoder unblocked (keeper swap-out, §9.2).
13. **Protocol evolution** — additive JSON fields are free; `protocol` bump
    reserved for breaking changes; daemon and client ship in one binary so
    skew only occurs across upgrades while a daemon is running — the
    handshake catches it (§8.2).
14. **Exclusive future ops** (e.g. `record start/stop`) may not overlap;
    the `E_BUSY` code and a per-op exclusivity flag in the dispatcher are
    specified now so the protocol doesn't need a breaking change later.

---

## 15. Implementation plan

Phased so that each lands independently green:

**Phase 0 — session refactor (no behavior change)**
Extract from `app/src/scrcpy.c`: `sc_session` module
(`app/src/session.{c,h}`) owning setup/run/teardown; `scrcpy()` becomes a
thin wrapper. Move `feat/control` executors to `app/src/control_exec.{c,h}`.
~400 LOC moved, ~100 new. Risk: highest of all phases; mitigated by
one-shot-equivalence testing (manual: window run, `--screencap`, `--control`,
record, disconnect handling).

**Phase 1 — daemon core**
`app/src/daemon/daemon.{c,h}` (listener, connection threads, dispatcher,
supervisor state machine), `app/src/daemon/frame_keeper.{c,h}`,
`app/src/daemon/registry.{c,h}`, `app/src/daemon/protocol.{c,h}` (framing +
jsmn parsing + response building), CLI options + validation matrix. `ping`,
`status`, `shutdown`, `screencap`, `control` ops. ~1,200 LOC new.

**Phase 2 — thin client**
`app/src/daemon/client.c`: CLI mapping (§8.6), registry resolution, payload
write-out, exit codes. ~350 LOC.

**Phase 3 — robustness + docs**
Reconnect backoff polish, `--daemon-reconnect`, freshness/`RESET_VIDEO`,
`E_BUSY` scaffolding, `doc/daemon.md` user documentation (this file gains a
user-facing section), bash/zsh completion updates, unit tests for protocol
framing and the registry (pure functions — fit the existing
`app/tests/` meson test harness).

Total new/moved code ≈ 2,300–2,800 LOC, all client-side C, no server (Java)
changes, no new runtime dependencies (jsmn is vendored, single header).

---

## 16. Future work (explicit non-goals of v1)

- `wait_ready` blocking op; `--daemon-list` client command.
- Frame/event *subscription* streams over IPC (push instead of poll).
- On-demand recording control (`record start/stop`) using the exclusivity
  scaffolding (§14.14).
- Daemon with a live mirroring window (lift §6.2), including handing the
  window over between reconnects.
- Unix-domain sockets / named pipes as an optional transport.
- Token-based auth for multi-user machines.
- App-launch (`--start-app`), file push, clipboard get/set as protocol ops —
  all straightforward `control`-socket messages once the dispatcher exists.
