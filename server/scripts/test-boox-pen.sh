#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
core="$root/server/src/main/java/com/genymobile/scrcpy/control/BooxPen.java"
if [[ ! -f "$core" ]]; then
    printf '%s\n' 'FAIL: BOOX evdev backend is not implemented' >&2
    exit 1
fi
javac -Xlint:all -Werror -d "$build" "$core" \
    "$root/server/src/main/java/com/genymobile/scrcpy/control/BooxDevice.java" \
    "$root/server/src/main/java/com/genymobile/scrcpy/control/BooxPenInitialization.java" \
    "$root/server/src/main/java/com/genymobile/scrcpy/control/BooxPenStartup.java" \
    "$root/server/src/main/java/com/genymobile/scrcpy/control/BooxPenPreparation.java" \
    "$root/server/src/main/java/com/genymobile/scrcpy/control/BooxMapping.java" \
    "$root/server/src/main/java/com/genymobile/scrcpy/control/BooxPenSession.java" \
    "$root"/server/src/testHost/java/com/genymobile/scrcpy/control/*Test.java
python3 "$root/server/scripts/test-boox-startup.py"
python3 "$root/server/scripts/test-boox-wait.py"
python3 "$root/server/scripts/test-boox-parent.py"
java -cp "$build" com.genymobile.scrcpy.control.BooxPenTest
java -cp "$build" com.genymobile.scrcpy.control.BooxPenEncoderTest
java -cp "$build" com.genymobile.scrcpy.control.BooxPenInitializationTest
java -cp "$build" com.genymobile.scrcpy.control.BooxPenStartupTest
java -cp "$build" com.genymobile.scrcpy.control.BooxPenPreparationTest \
    "$root/server/src/testHost/java/com/genymobile/scrcpy/control/fixtures/boox-input.txt"
java -cp "$build" com.genymobile.scrcpy.control.BooxDeviceTest
java -cp "$build" com.genymobile.scrcpy.control.BooxPenSessionTest
java -cp "$build" com.genymobile.scrcpy.control.BooxPenBufferTest
java -cp "$build" com.genymobile.scrcpy.control.BooxMappingTest \
    "$root/server/src/testHost/java/com/genymobile/scrcpy/control/fixtures/boox-input.txt"
