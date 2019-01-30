#!/bin/bash
set -e

# build and test locally
BUILDDIR=build_release
rm -rf "$BUILDDIR"
meson "$BUILDDIR" --buildtype release --strip -Db_lto=true
cd "$BUILDDIR"
ninja
ninja test
cd -

# build Windows releases
make -f Makefile.CrossWindows

# the generated server must be the same everywhere
cmp "$BUILDDIR/server/scrcpy-server.jar" dist/scrcpy-win32/scrcpy-server.jar
cmp "$BUILDDIR/server/scrcpy-server.jar" dist/scrcpy-win64/scrcpy-server.jar

# get version name
TAG=$(git describe --tags --always)

# create release directory
mkdir -p "release-$TAG"
cp "$BUILDDIR/server/scrcpy-server.jar" "release-$TAG/scrcpy-server-$TAG.jar"
cp "dist/scrcpy-win32-$TAG.zip" "release-$TAG/"
cp "dist/scrcpy-win64-$TAG.zip" "release-$TAG/"

# generate checksums
cd "release-$TAG"
sha256sum "scrcpy-server-$TAG.jar" \
          "scrcpy-win32-$TAG.zip" \
          "scrcpy-win64-$TAG.zip" > SHA256SUMS.txt

echo "Release generated in release-$TAG/"
