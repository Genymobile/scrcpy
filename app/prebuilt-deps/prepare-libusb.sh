#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=libusb-1.0.25

FILENAME=libusb-1.0.25.7z
SHA256SUM=3d1c98416f454026034b2b5d67f8a294053898cb70a8b489874e75b136c6674d

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/libusb/libusb/releases/download/v1.0.25/$FILENAME" "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

7z x "../$FILENAME" \
    MinGW32/dll/libusb-1.0.dll \
    MinGW64/dll/libusb-1.0.dll \
    include /
