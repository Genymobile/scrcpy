#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

VERSION=1.0.27
DEP_DIR="libusb-$VERSION"

FILENAME="libusb-$VERSION.7z"
SHA256SUM=19835e290f46fab6bd8ce4be6ab7dc5209f1c04bad177065df485e51dc4118c8

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/libusb/libusb/releases/download/v$VERSION/$FILENAME" \
    "$FILENAME" "$SHA256SUM"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

mkdir tmp
cd tmp
7z x "../../$FILENAME" \
    "include/" \
    "MinGW32/" \
    "MinGW64/"
cd ..

for dir in MinGW32 MinGW64
do
    mkdir -p "$dir"
    cd "$dir"
    mkdir -p include/libusb-1.0 bin lib
    cp -r ../tmp/include/libusb.h include/libusb-1.0/
    cp ../tmp/"$dir"/dll/libusb-1.0.dll bin/
    cp ../tmp/"$dir"/static/libusb-1.0.dll.a lib/
    mkdir lib/pkgconfig
    cat > lib/pkgconfig/libusb-1.0.pc << "EOF"
prefix=/home/appveyor/$dir
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: libusb-1.0
Description: C API for USB device access from Linux, Mac OS X, Windows, OpenBSD/NetBSD and Solaris userspace
Version: 1.0.27
Libs: -L${libdir} -lusb-1.0
Libs.private: 
Cflags: -I${includedir}/libusb-1.0
EOF
    cd ..
done

rm -rf tmp
