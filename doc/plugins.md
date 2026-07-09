# Plugin management

Add-ons (see [addons.md](addons.md)) are entrypoint scripts. This page covers
**installing** them from a git repository and loading them by **name** instead
of an absolute path.

## Where plugins live

Installed plugins live under a fixed directory, one sub-directory each:

    ~/.scrcpy-auto/plugins/<name>/

Each `<name>/` is a self-contained add-on directory. Its entrypoint **must be
`entrypoint.sh`** (executable) — that is what `--add-on=<name>` resolves to. A
plugin bundles everything it needs; it is fine to duplicate shared helpers (e.g.
`ai_common.py`) into each plugin that uses them.

## Installing

    scrcpy-auto plugins-install <git-url> --add-on-name=<name>

`<git-url>` is any git repository (HTTPS or SSH). A single repository may hold
several plugins in different sub-directories:

    scrcpy-plugins/
    ├── adb/
    ├── exec_prompt/
    ├── assert_prompt/
    └── video_assert_prompt/

Installing copies the matching sub-directory into `~/.scrcpy-auto/plugins/`.
`--add-on-name=ALL` installs **every** plugin directory in the repository:

    scrcpy-auto plugins-install https://example.com/you/scrcpy-plugins --add-on-name=exec_prompt
    scrcpy-auto plugins-install git@example.com:you/scrcpy-plugins.git  --add-on-name=ALL

## Upgrading

    scrcpy-auto plugins-upgrade <git-url> --add-on-name=<name>

Re-fetches the repository and replaces the installed copy. With
`--add-on-name=ALL`, every plugin from that repository that is **already
installed** is refreshed (new plugins are not added — use `plugins-install ALL`
for that).

## Loading by name

Once installed, `--add-on` accepts the bare plugin name instead of a path:

    scrcpy-auto -s DEVICEID --daemon-port 27400 --no-window --add-on=exec_prompt

The name is resolved to `~/.scrcpy-auto/plugins/exec_prompt/entrypoint.sh`. A
value that looks like a path (contains `/`, or names an existing file) is still
used as-is, so `--add-on=./local-entrypoint.sh` keeps working.

## Notes

- Plugins are plain static files; install/upgrade is just a shallow `git clone`
  plus a directory copy (`cp -R`). The scratch clone lives at
  `~/.scrcpy-auto/.clone-tmp` and is removed afterwards.
- Both commands replace the destination directory, so re-running is idempotent.
- Plugin management is a Unix/macOS feature, like add-ons themselves; on Windows
  the subcommands are no-ops.
