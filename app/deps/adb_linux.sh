#!/usr/bin/env bash
set -ex
. $(dirname ${BASH_SOURCE[0]})/_init "$@"

VERSION=36.0.0
URL="https://dl.google.com/android/repository/platform-tools_r$VERSION-linux.zip"
SHA256SUM=0ead642c943ffe79701fccca8f5f1c69c4ce4f43df2eefee553f6ccb27cbfbe8

PROJECT_DIR="platform-tools-$VERSION-linux"
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

mkdir -p "$INSTALL_DIR/adb-linux"
cd "$INSTALL_DIR/adb-linux"
cp -r "$SOURCES_DIR/$PROJECT_DIR"/. "$INSTALL_DIR/adb-linux/"
