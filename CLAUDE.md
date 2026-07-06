# scrcpy-auto (fork of Genymobile/scrcpy)

This is NOT upstream scrcpy. It is a fork renamed to `scrcpy-auto`, adding
automation features. **Read `doc/fork.md` before resolving any merge conflict
or touching the files listed there** — it is the authoritative inventory of
every divergence from upstream and the per-hotspot resolution rules.

- `master` mirrors upstream; `feat/control` carries all fork work.
- Daemon mode design/spec: `doc/daemon.md`.

## Invariants that must survive every upstream merge

Never resolve a conflict by taking only the upstream side in a file that
`doc/fork.md` lists as modified. In particular:

1. Binary names: client `scrcpy-auto`, server `scrcpy-auto-server`, device
   path `/data/local/tmp/scrcpy-auto-server.jar`, install dir
   `share/scrcpy-auto/` (defines at the top of `app/src/server.c`; names in
   `app/meson.build` and `server/meson.build`).
2. Fork CLI options and their plumbing in `app/src/cli.c`,
   `app/src/options.{h,c}`: `--screencap`, `--control`, `--daemon-port`,
   `--client-port`, `--daemon-host`, `--json`, `--daemon-stop`,
   `--daemon-status`, `--daemon-reconnect`, `--auto-test-report`, `--action`,
   `--note`, `--add-on`, plus the unknown-`--name=value` plugin-call pre-scan in
   `scrcpy_parse_args`.
3. Fork events in `app/src/events.h` (`SC_EVENT_SCREENCAP_*`).
4. `SC_PACKET_SOURCE_MAX_SINKS` ≥ 3 in `app/src/trait/packet_source.h`.
5. `libswscale` dependency and the fork source files in `app/meson.build`.
6. Fork-owned modules (`app/src/daemon/`, `app/src/control_exec.*`,
   `app/src/screencap.*`) compile against upstream-internal APIs — a merge
   can break them with NO textual conflict. Two known traps:
   - packet/frame sink `open()` signatures change across upstream versions
     (happened in 4.0);
   - `app/src/daemon/daemon.c` duplicates `scrcpy.c`'s
     `struct sc_server_params` initializer — mirror any new field there.

## After any upstream merge

Run the checklist in `doc/fork.md` §6: no conflict markers, meson parses,
full tree compiles with zero `-Wall -Wextra` warnings (a toolchain-free
zig-cc recipe is documented there), invariant greps pass, then a device
smoke test of `--screencap`, `--control`, and the daemon flow.

## Conventions

- C11, upstream scrcpy style (4-space indent, `sc_` prefixes, one-shot
  goto-cleanup ladders). Match surrounding code.
- The tree must stay warning-free under `-Wall -Wextra`.
- Do not add external runtime dependencies; small vendored single-header
  libraries go to `app/src/third_party/` (jsmn lives there).
- Do not modify `release/` packaging scripts to fork names without also
  testing them; they intentionally still use upstream names (doc/fork.md §3.2).
