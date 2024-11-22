#!/bin/bash
cd "$(dirname ${BASH_SOURCE[0]})"
export ADB=./adb
export SCRCPY_SERVER_PATH=./scrcpy-server
export ICON=./icon.png
scrcpy "$@"
