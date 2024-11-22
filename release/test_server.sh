#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common
cd .. # root project dir

GRADLE="${GRADLE:-./gradlew}"

"$GRADLE" -p server check
