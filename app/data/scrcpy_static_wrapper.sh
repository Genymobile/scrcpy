#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
export ADB="${ADB:-./adb}"
export SCRCPY_SERVER_PATH="${SCRCPY_SERVER_PATH:-./scrcpy-server}"
export SCRCPY_ICON_PATH="${SCRCPY_ICON_PATH:-./icon.png}"
./scrcpy_bin "$@"
