#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common
cd .. # root project dir

if [[ $# != 1 ]]
then
    # <target_name>: for example win64
    echo "Syntax: $0 <target>" >&2
    exit 1

fi

BUILD_DIR="$WORK_DIR/build-$1"
ARCHIVE_DIR="$BUILD_DIR/release-archive"
TARGET="scrcpy-$1-$VERSION"

rm -rf "$ARCHIVE_DIR/$TARGET"
mkdir -p "$ARCHIVE_DIR/$TARGET"

cp -r "$BUILD_DIR/dist/." "$ARCHIVE_DIR/$TARGET/"
cp "$WORK_DIR/build-server/server/scrcpy-server" "$ARCHIVE_DIR/$TARGET/"

mkdir -p "$OUTPUT_DIR"

cd "$ARCHIVE_DIR"
rm -f "$OUTPUT_DIR/$TARGET.zip"
zip -r "$OUTPUT_DIR/$TARGET.zip" "$TARGET"
rm -rf "$TARGET"
cd -
echo "Generated '$OUTPUT_DIR/$TARGET.zip'"
