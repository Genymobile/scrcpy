# Plugin add-ons

Add-ons let external tools provide custom `--<command>` options for
`scrcpy-auto` without modifying its C code. An add-on is a shell entrypoint
script; it is meant for logic that does not belong in the core (AI workflows,
Python integrations, custom automation), which can then reuse scrcpy-auto for
screenshots and input.

> **Installing add-ons by name.** Add-ons can be installed from a git
> repository and loaded by name (`--add-on=<name>`) instead of a path —
> see [plugins.md](plugins.md).

## Loading (daemon side)

Load one or more entrypoints when starting the daemon:

```bash
scrcpy-auto -s DEVICEID --daemon-port 27400 --no-window \
            --auto-test-report=./test-run-1 \
            --add-on=./prompt-entrypoint.sh
```

At startup the daemon **queries** each script for the command name it provides
(see the contract below) and registers `name → script`. `--add-on` may be
given multiple times. A command that collides with a built-in option is simply
invalid — no automatic conflict detection is performed; the plugin author must
avoid collisions.

## Invoking (client side)

```bash
scrcpy-auto --client-port 27400 --prompt="Click the Login button"
```

Any `--NAME=VALUE` the built-in parser does not recognize is treated as a
plugin call and forwarded to the daemon, which runs the registered add-on for
`NAME` with `VALUE`. The `=` form is required (`--name=value`). If no add-on
provides `NAME`, the client reports an error.

An add-on may also declare **extra named arguments** beyond its primary value
(see the entrypoint contract). The client discovers these from the daemon (the
daemon advertises every add-on's schema in its hello), so it knows that, e.g.,
`--ref-images` belongs to the `--prompt` add-on and groups them into one call:

```bash
scrcpy-auto --client-port 27400 \
  --prompt="Click the Like button" \
  --ref-images=~/ref/like_button.png \
  --ref-images=~/ref/like_alt.png
```

A `list` argument may be repeated (as above) or comma-separated
(`--ref-images=a.png,b.png`); a non‑list argument may appear only once. An
argument given without its command (`--ref-images` but no `--prompt`), an
unknown `--flag`, or a command repeated in one call are all rejected client‑side
with a clear message.

#### Shared argument names — the qualified `--<command>.<arg>` form

Different add-ons may declare arguments with the **same name** (e.g. both
`--exec_prompt` and `--assert_prompt` accept `--ref-images`). A **bare**
`--ref-images` is routed to the invoked add-on that declares it, which is
unambiguous as long as only one such add-on is invoked in the call. When **more
than one** invoked add-on declares the same argument, the bare form is rejected
as ambiguous, and each value must be attached to its command with the
**qualified** `--<command>.<arg>` form:

```bash
scrcpy-auto --client-port 27400 \
  --exec_prompt="Tap the Like button"   --exec_prompt.ref-images=~/ref/like.png \
  --assert_prompt="Is it now liked?"    --assert_prompt.ref-images=~/ref/liked.png
```

The prefix before the first `.` must match an invoked command; the suffix must
be one of that command's declared arguments (both are validated client‑side).
The daemon still receives the plain argument (`SC_ARG_REF_IMAGES`) — the
`<command>.` prefix is only a client-side routing hint and is stripped before
the call is sent, so entrypoint scripts need no changes to benefit from it.

### Reference-image transport (`path`/`pathlist` args)

An argument declared as `path`/`pathlist` holds files the **daemon** must read.
The client picks the optimal strategy automatically, based on `--daemon-host`
(default `127.0.0.1`):

- **Local daemon** (loopback host): the paths are sent as‑is and the daemon
  reads the files directly — no copy.
- **Remote daemon**: the client reads each file and **uploads the bytes** over
  the connection (the `upload` op, §7 of doc/daemon.md); the daemon stores them
  in a per‑connection temp file and the plugin receives those daemon‑side paths.
  The daemon never needs access to the client's filesystem.

Either way the plugin only ever sees paths in `SC_ARG_<NAME>` and is oblivious
to locality.

### Machine-readable output (`--json`)

With `--json`, human logs are suppressed and the client prints the plugin's
**result object** (see below) to stdout; on failure it prints
`{"error":{…}}` and exits non‑zero. This is the interface for AI/automation
harnesses:

```bash
scrcpy-auto --client-port 27400 --json \
  --prompt="Click the Login button" --ref-images=~/ref/login.png
```

You can combine a plugin call with `--note` to label it in the report:

```bash
scrcpy-auto --client-port 27400 \
  --prompt="Click the Login button" \
  --note="AI Prompt: Click the Login button"
```

## Entrypoint contract

The daemon runs the script with the environment variable `SC_ADDON_MODE` set:

- **`register`** — print the command **metadata** to stdout and exit 0. Called
  once at daemon startup. The output is line based (Unified Plugin Protocol);
  unknown keys are ignored (forward-compatible):

  ```
  name=<command>                                       # the --<command> option
  result=<field>                                       # result field name (opt.)
  arg=<name>:<string|list|path|pathlist>:<required|optional>   # extra arg (0+)
  env=<NAME>:<required|optional>[:<description>]        # declared env var (0+)
  meta=<key>:<value>                                    # free-form metadata (0+)
  ```

  The command's own value (`--<command>=VALUE`) is an implicit required string
  argument, so only *additional* arguments need an `arg=` line. `list`/`pathlist`
  accept multiple values (repeated flag or comma-separated); `path`/`pathlist`
  mark file paths (adaptive transport, above). A `required` `env` that is unset
  in the daemon's environment fails the run (`E_PLUGIN`). For back-compat, a bare
  command name on its own line (no `name=`) is accepted and declares nothing
  else. Each declaration is surfaced to the client (flag grouping) and the web
  UI (panel) via the daemon hello.

- **`run`** — perform the operation. `$1` is the command value. The daemon
  waits for the script to exit; exit 0 = success, non‑zero = failure (returned
  to the client).

In `run` mode the script receives these environment variables:

| Variable | Meaning |
|---|---|
| `SC_DAEMON_PORT` | the daemon's `--daemon-port` |
| `SC_DAEMON_HOST` | `127.0.0.1` |
| `SC_DEVICE_SERIAL` | the connected device serial |
| `SC_REPORT_DIR` | the `--auto-test-report` directory (empty if none) |
| `SC_VIDEO_WIDTH`, `SC_VIDEO_HEIGHT` | current video size (0 if no frame yet) |
| `SC_VIDEO_CODEC` | video codec: `h264` / `h265` / `av1` (empty if `--no-video`) |
| `SC_VIDEO_BIT_RATE` | `--video-bit-rate` in bps (0 = server default) |
| `SC_VIDEO_MAX_FPS` | `--max-fps` (empty if unset) |
| `SC_VIDEO_MAX_SIZE` | `--max-size` in px (0 = unlimited) |
| `SC_VIDEO_ENCODER` | `--video-encoder` name (empty if unset) |
| `SC_CONTROL` | `1` if input injection is enabled, else `0` |
| `SC_ADDON_NAME` | the command name being run |
| `SCRCPY_AUTO` | path to the scrcpy-auto binary (for callbacks) |
| `SC_ARG_<NAME>` | each declared argument's value (name upper-cased, `-`→`_`); the primary is also `SC_ARG_<COMMAND>` |
| `SC_RESULT_FILE` | path to write the structured result JSON to (see below) |

`list`/`pathlist` arguments arrive newline-separated in their `SC_ARG_<NAME>`.
For example `--prompt="hi" --ref-images=a.png --ref-images=b.png` yields
`$1`=`hi`, `SC_ARG_PROMPT`=`hi`, and `SC_ARG_REF_IMAGES`=`a.png\nb.png`.

### Returning a result (`SC_RESULT_FILE`)

To return structured data to the caller (for `--json` and the web UI), the
script writes a JSON object to `$SC_RESULT_FILE`. The daemon reads it back after
the script exits and attaches it to the response as `result`. By convention a
result contains at least `{"result":…,"reason":…,"thinking":…}` (extensions
allowed). A script that writes nothing simply returns no `result`.

The script does its real work by calling back into scrcpy-auto, e.g.
`"$SCRCPY_AUTO" --client-port "$SC_DAEMON_PORT" --screencap out.png`.

Requirements: the entrypoint must be executable (`chmod +x`). Add-ons are a
Unix/macOS feature (the daemon launches them via `env`); Windows is not
supported in v1.

## Long-running "service" add-ons

A normal add-on is one-shot: the daemon runs the script and blocks until it
exits. A **service** add-on may instead **keep running in the background** —
useful for a live report renderer, a web UI, a metrics exporter, etc. The
daemon launches it, tracks its PID, and **terminates it automatically when the
daemon shuts down** (SIGTERM, then SIGKILL after a short grace period).

Declare it in `register` output:

```
name=stream-report-render
service=true
arg=port:string:optional
arg=export:string:optional
```

Protocol (all handled by the daemon, no client changes needed):

1. A client invokes the command as usual
   (`--stream-report-render=true --stream-report-render.port=8001`).
2. In `run` mode the script starts its background work (e.g. binds a port),
   then **writes its result to `$SC_RESULT_FILE` to signal "ready"** and keeps
   running (does *not* exit).
3. The daemon waits until the result file is written (or the process exits),
   returns that result to the client, and — if the process is **still alive** —
   **adopts** it: the process is now owned by the daemon and lives until the
   daemon stops.

The same service script can also do **one-shot** work in the same `run` mode
(for example `--stream-report-render.export=./out`): just do the work, write
the result, and **exit**. The daemon sees it exit and treats it as an ordinary
one-shot call — nothing is adopted. So the distinction is behavioural (did the
process stay alive?), not a separate mode.

Single-instance is naturally enforced by whatever resource the service holds:
if a second invocation cannot bind the same port, it should write a result like
`{"result":"already-serving"}` and exit 0 (so the daemon does not adopt a
duplicate).

Up to `SC_MAX_SERVICES` (8) services may be adopted at once.

## Report integration and timestamps

When `--auto-test-report` is active, the daemon logs a `plugin` start event
the moment the add-on begins (so it always appears on the timeline, even
without `--note`). The add-on's own screenshot / note / click steps are
separate client requests, each logged **when it happens** — the report never
waits for the whole operation to finish (see `DESIGN-test-report.md` §6). A
typical AI operation therefore appears as:

```
… plugin (--prompt "Click Login")   ← operation starts
… screencap                          ← add-on captured a frame
… note   (analysis: found at 290,690)
… control (click 290 690 100)        ← add-on clicked
```

The web report renders `plugin` events with a 🧩 marker.

## Lifecycle & security

- The client call blocks until the add-on exits; the add-on runs on the
  daemon, which sets the environment above.
- On daemon shutdown (or `--daemon-stop`) a running add-on child is
  terminated.
- Add-ons are arbitrary shell that the daemon **operator** explicitly loads
  with `--add-on`; any local client of the daemon can then invoke a loaded
  add-on. This is the same localhost trust boundary as the rest of the daemon
  — do not load untrusted scripts, and do not expose the daemon beyond
  localhost.

## Example: an AI `--prompt` add-on

`prompt-entrypoint.sh` — screenshot → multimodal LLM → click:

```sh
#!/bin/sh
# Provides: --prompt="<instruction>" [--ref-images=<path>[,<path>...]]
case "$SC_ADDON_MODE" in
  register)
    printf 'name=prompt\n'
    printf 'arg=ref-images:list:optional\n'
    exit 0 ;;
esac

PROMPT="$1"                      # also $SC_ARG_PROMPT
SC="$SCRCPY_AUTO --client-port $SC_DAEMON_PORT"

# 1. capture the current screen
shot="${SC_REPORT_DIR:-/tmp}/prompt-$$.png"
$SC --screencap "$shot" || exit 1

# 2. ask the model where to act, passing the screenshot + any reference images
#    ($SC_ARG_REF_IMAGES is newline-separated; empty if --ref-images was omitted)
coords=$(python3 "$(dirname "$0")/analyze.py" "$shot" "$PROMPT") || exit 1

# 3. record the AI's decision, then perform the action
$SC --note="analysis: $PROMPT -> $coords"
$SC --control="click $coords 100"
```

Run it:

```bash
scrcpy-auto --client-port 27400 --prompt="Click the Like button" \
            --ref-images=~/ref/like_button.png \
            --note="AI Prompt: Click the Like button"
```

A complete stdlib-only reference implementation (`prompt.py`) that sends the
screenshot **and** the reference images to an OpenAI-compatible multimodal
endpoint lives outside this repo; the entrypoint above is the minimal shape.
