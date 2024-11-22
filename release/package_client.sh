#!/bin/bash
set -ex
cd "$(dirname ${BASH_SOURCE[0]})"
. build_common
cd .. # root project dir

if [[ $# != 2 ]]
then
    # <target_name>: for example win64
    # <format>: zip or tar.gz
    echo "Syntax: $0 <target> <format>" >&2
    exit 1
fi

FORMAT=$2

if [[ "$2" != zip && "$2" != tar.gz ]]
then
    echo "Invalid format (expected zip or tar.gz): $2" >&2
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
rm -f "$OUTPUT_DIR/$TARGET.$FORMAT"

case "$FORMAT" in
    zip)
        zip -r "$OUTPUT_DIR/$TARGET.zip" "$TARGET"
        ;;
    tar.gz)
        tar cvf "$OUTPUT_DIR/$TARGET.tar.gz" "$TARGET"
        ;;
    *)
        echo "Invalid format (expected zip or tar.gz): $FORMAT" >&2
        exit 1
esac

rm -rf "$TARGET"
cd -
echo "Generated '$OUTPUT_DIR/$TARGET.$FORMAT'"
