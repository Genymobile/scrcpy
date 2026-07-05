# Plugin add-ons

Add-ons let external tools provide custom `--<command>` options for
`scrcpy-auto` without modifying its C code. An add-on is a shell entrypoint
script; it is meant for logic that does not belong in the core (AI workflows,
Python integrations, custom automation), which can then reuse scrcpy-auto for
screenshots and input.

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
provides `NAME`, the daemon returns an error.

You can combine a plugin call with `--note` to label it in the report:

```bash
scrcpy-auto --client-port 27400 \
  --prompt="Click the Login button" \
  --note="AI Prompt: Click the Login button"
```

## Entrypoint contract

The daemon runs the script with the environment variable `SC_ADDON_MODE` set:

- **`register`** — print the command name (one line) to stdout and exit 0.
  Called once at daemon startup.
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
| `SC_ADDON_NAME` | the command name being run |
| `SCRCPY_AUTO` | path to the scrcpy-auto binary (for callbacks) |

The script does its real work by calling back into scrcpy-auto, e.g.
`"$SCRCPY_AUTO" --client-port "$SC_DAEMON_PORT" --screencap out.png`.

Requirements: the entrypoint must be executable (`chmod +x`). Add-ons are a
Unix/macOS feature (the daemon launches them via `env`); Windows is not
supported in v1.

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
# Provides: --prompt="<instruction>"
case "$SC_ADDON_MODE" in
  register) echo "prompt"; exit 0 ;;
esac

PROMPT="$1"
SC="$SCRCPY_AUTO --client-port $SC_DAEMON_PORT"

# 1. capture the current screen
shot="${SC_REPORT_DIR:-/tmp}/prompt-$$.png"
$SC --screencap "$shot" || exit 1

# 2. ask the model where to act (your program prints "X Y")
coords=$(python3 "$(dirname "$0")/analyze.py" "$shot" "$PROMPT") || exit 1

# 3. record the AI's decision, then perform the action
$SC --note="analysis: $PROMPT -> $coords"
$SC --control="click $coords 100"
```

Run it:

```bash
scrcpy-auto --client-port 27400 --prompt="Click the Login button" \
            --note="AI Prompt: Click the Login button"
```
