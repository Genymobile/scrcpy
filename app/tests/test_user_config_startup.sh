#!/bin/sh
set -eu

scrcpy_auto=$1
test_home=$(mktemp -d "${TMPDIR:-/tmp}/scrcpy-auto-startup-test.XXXXXX")

cleanup() {
    rmdir "$test_home/.scrcpy-auto" 2>/dev/null || true
    rmdir "$test_home" 2>/dev/null || true
}
trap cleanup 0 HUP INT TERM

HOME="$test_home" "$scrcpy_auto" --version >/dev/null
test -d "$test_home/.scrcpy-auto"

# A second startup must remain idempotent.
HOME="$test_home" "$scrcpy_auto" --help >/dev/null
test -d "$test_home/.scrcpy-auto"
