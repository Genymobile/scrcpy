#!/bin/sh
set -e

mkdir build
cd build
meson .. --wrap-mode=nodownload -Dcompile_server=false
ninja test
rm -rf build
