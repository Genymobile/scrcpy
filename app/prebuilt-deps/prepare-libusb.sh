#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

VERSION=1.0.26
DEP_DIR="libusb-$VERSION"

FILENAME="libusb-$VERSION-binaries.7z"
SHA256SUM=9c242696342dbde9cdc47239391f71833939bf9f7aa2bbb28cdaabe890465ec5

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/libusb/libusb/releases/download/v$VERSION/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

7z x "../$FILENAME" \
    "libusb-$VERSION-binaries/libusb-MinGW-Win32/" \
    "libusb-$VERSION-binaries/libusb-MinGW-Win32/" \
    "libusb-$VERSION-binaries/libusb-MinGW-x64/" \
    "libusb-$VERSION-binaries/libusb-MinGW-x64/"

mv "libusb-$VERSION-binaries/libusb-MinGW-Win32" .
mv "libusb-$VERSION-binaries/libusb-MinGW-x64" .
rm -rf "libusb-$VERSION-binaries"

# Rename the dll to get the same library name on all platforms
mv libusb-MinGW-Win32/bin/msys-usb-1.0.dll libusb-MinGW-Win32/bin/libusb-1.0.dll
mv libusb-MinGW-x64/bin/msys-usb-1.0.dll libusb-MinGW-x64/bin/libusb-1.0.dll
