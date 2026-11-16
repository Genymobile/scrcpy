#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init "$@"

VERSION=37.0.1
URL="https://dl.google.com/android/repository/platform-tools_r$VERSION-darwin.zip"
SHA256SUM=ee39ad5967e95c2a07f04dbcbde96b1a0c916ba376096db5d2f498b7727a5d1d

PROJECT_DIR="platform-tools-$VERSION-darwin"
FILENAME="$PROJECT_DIR.zip"

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "$URL" "$FILENAME" "$SHA256SUM"
    mkdir -p "$PROJECT_DIR"
    cd "$PROJECT_DIR"
    ZIP_PREFIX=platform-tools
    unzip "../$FILENAME" "$ZIP_PREFIX"/adb
    mv "$ZIP_PREFIX"/* .
    rmdir "$ZIP_PREFIX"
fi

mkdir -p "$INSTALL_DIR/adb-macos"
cd "$INSTALL_DIR/adb-macos"
cp -r "$SOURCES_DIR/$PROJECT_DIR"/. "$INSTALL_DIR/adb-macos/"
