#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common
cd .. # root project dir

TEST_BUILD_DIR="$WORK_DIR/build-test"

rm -rf "$TEST_BUILD_DIR"
meson setup "$TEST_BUILD_DIR" -Dcompile_server=false \
    -Db_sanitize=address,undefined
ninja -C "$TEST_BUILD_DIR" test
