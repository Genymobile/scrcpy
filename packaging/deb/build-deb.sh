#!/usr/bin/env bash
#
# build-deb.sh — produce a self-contained scrcpy-auto .deb using Docker.
#
# This is the deliverable entry point. It needs only Docker on the host (no
# local C toolchain): it spins up a clean ubuntu:24.04 build container, runs
# build-in-container.sh inside it, and drops the finished package in ./dist.
#
# Usage:
#   packaging/deb/build-deb.sh [options]
#
# Options:
#   --output DIR      where to place the .deb            (default: <repo>/dist)
#   --image IMAGE     build/base image                   (default: ubuntu:24.04)
#   --server MODE     prebuilt | none | /abs/path/to/server   (default: prebuilt)
#   --no-adb          do not bundle adb
#   --version VER     override the .deb version string
#   --test            after building, run the offline install test
#   --run-tests       run the unit tests (meson test) during the build
#   -h, --help        show this help
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)

IMAGE=ubuntu:24.04
OUTDIR="$REPO/dist"
SERVER_MODE=prebuilt
BUNDLE_ADB=1
DEB_VERSION=""
DO_TEST=0
RUN_TESTS=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output)  OUTDIR=$(cd "$2" 2>/dev/null && pwd || echo "$2"); shift 2 ;;
        --image)   IMAGE="$2"; shift 2 ;;
        --server)  case "$2" in
                       prebuilt|none) SERVER_MODE="$2" ;;
                       /*)            SERVER_MODE="path:$2" ;;
                       *) echo "--server must be prebuilt|none|/abs/path" >&2; exit 1 ;;
                   esac; shift 2 ;;
        --no-adb)  BUNDLE_ADB=0; shift ;;
        --version) DEB_VERSION="$2"; shift 2 ;;
        --test)    DO_TEST=1; shift ;;
        --run-tests) RUN_TESTS=1; shift ;;
        -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

command -v docker >/dev/null || { echo "docker is required" >&2; exit 1; }
mkdir -p "$OUTDIR"

# Persistent cache for downloaded + compiled dependencies (SDL/FFmpeg/dav1d/
# libusb). Lives outside the repo so it survives source re-syncs. Speeds up
# re-runs enormously; safe to delete.
CACHE="${CACHE:-$HOME/.cache/scrcpy-auto-deb}"
mkdir -p "$CACHE"

# If a local server path was given, expose it to the container.
SERVER_MOUNT=()
SERVER_MODE_IN="$SERVER_MODE"
if [[ "$SERVER_MODE" == path:* ]]; then
    host_path="${SERVER_MODE#path:}"
    [[ -f "$host_path" ]] || { echo "server file not found: $host_path" >&2; exit 1; }
    SERVER_MOUNT=(-v "$host_path":/server-in:ro)
    SERVER_MODE_IN="path:/server-in"
fi

echo "==> Building scrcpy-auto .deb in $IMAGE (output: $OUTDIR)"
docker run --rm \
    -e SERVER_MODE="$SERVER_MODE_IN" \
    -e BUNDLE_ADB="$BUNDLE_ADB" \
    -e DEB_VERSION="$DEB_VERSION" \
    -e RUN_TESTS="$RUN_TESTS" \
    -e SRC=/src -e OUT=/out -e BUILD=/build \
    -v "$REPO":/src:ro \
    -v "$OUTDIR":/out \
    -v "$CACHE":/build/app/deps/work \
    "${SERVER_MOUNT[@]}" \
    "$IMAGE" bash /src/packaging/deb/build-in-container.sh

DEB=$(ls -t "$OUTDIR"/scrcpy-auto_*_amd64.deb 2>/dev/null | head -1)
[[ -n "$DEB" ]] || { echo "No .deb produced" >&2; exit 1; }
echo "==> Built: $DEB"

if [[ "$DO_TEST" == 1 ]]; then
    echo "==> Running offline install test"
    IMAGE="$IMAGE" bash "$HERE/test-deb.sh" "$DEB"
fi
