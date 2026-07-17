#!/usr/bin/env bash
#
# test-deb.sh — install the .deb in a pristine, OFFLINE ubuntu:24.04 container
# and verify it runs. `--network none` proves the package needs nothing from the
# internet or apt at install time.
#
# Usage: packaging/deb/test-deb.sh path/to/scrcpy-auto_<ver>_amd64.deb
set -euo pipefail

DEB="${1:?usage: test-deb.sh <path-to-.deb>}"
IMAGE="${IMAGE:-ubuntu:24.04}"
DEBDIR=$(cd "$(dirname "$DEB")" && pwd)
DEBFILE=$(basename "$DEB")

echo "==> Offline install test of $DEBFILE in $IMAGE (network disabled)"
docker run --rm --network none \
    -v "$DEBDIR":/deb:ro \
    -e DEBFILE="$DEBFILE" \
    "$IMAGE" bash -euo pipefail -c '
        echo "== dpkg -i (offline) =="
        if ! dpkg -i "/deb/$DEBFILE"; then
            echo "!! install failed (unmet deps below) !!"
            dpkg --configure -a || true
            exit 1
        fi

        echo; echo "== installed files =="
        dpkg -L scrcpy-auto | sort

        echo; echo "== binary type / linkage =="
        bin="$(command -v scrcpy-auto)"
        if command -v file >/dev/null; then file "$bin"; else
            head -c4 "$bin" | od -An -tx1 | grep -q "7f 45 4c 46" && echo "$bin: ELF executable"
        fi
        echo "-- ldd (should list only base libs) --"; ldd "$bin" || true

        echo; echo "== scrcpy-auto --version =="
        scrcpy-auto --version

        echo; echo "== scrcpy-auto --help (head) =="
        scrcpy-auto --help 2>&1 | head -n 15 || true

        echo; echo "== server present =="
        ls -l /usr/share/scrcpy-auto/

        echo; echo "== adb =="
        if command -v adb >/dev/null; then adb version | head -n1 || true; else echo "(adb not bundled)"; fi

        echo; echo "== purge (clean removal) =="
        dpkg -r scrcpy-auto >/dev/null && echo "removed cleanly"

        echo; echo "ALL-OK"
    '
