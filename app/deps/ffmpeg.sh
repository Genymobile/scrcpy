#!/usr/bin/env bash
set -ex
DEPS_DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DEPS_DIR"
. common

VERSION=7.0.1
FILENAME=ffmpeg-$VERSION.tar.xz
PROJECT_DIR=ffmpeg-$VERSION
SHA256SUM=bce9eeb0f17ef8982390b1f37711a61b4290dc8c2a0c1a37b5857e85bfb0e4ff

cd "$SOURCES_DIR"

if [[ -d "$PROJECT_DIR" ]]
then
    echo "$PWD/$PROJECT_DIR" found
else
    get_file "https://ffmpeg.org/releases/$FILENAME" "$FILENAME" "$SHA256SUM"
    tar xf "$FILENAME"  # First level directory is "$PROJECT_DIR"
fi

mkdir -p "$BUILD_DIR/$PROJECT_DIR"
cd "$BUILD_DIR/$PROJECT_DIR"

if [[ "$HOST" = win32 ]]
then
    ARCH=x86
elif [[ "$HOST" = win64 ]]
then
    ARCH=x86_64
else
    echo "Unsupported host: $HOST" >&2
    exit 1
fi

# -static-libgcc to avoid missing libgcc_s_dw2-1.dll
# -static to avoid dynamic dependency to zlib
export CFLAGS='-static-libgcc -static'
export CXXFLAGS="$CFLAGS"
export LDFLAGS='-static-libgcc -static'

if [[ -d "$HOST" ]]
then
    echo "'$PWD/$HOST' already exists, not reconfigured"
    cd "$HOST"
else
    mkdir "$HOST"
    cd "$HOST"

    "$SOURCES_DIR/$PROJECT_DIR"/configure \
        --prefix="$INSTALL_DIR/$HOST" \
        --enable-cross-compile \
        --target-os=mingw32 \
        --arch="$ARCH" \
        --cross-prefix="${HOST_TRIPLET}-" \
        --cc="${HOST_TRIPLET}-gcc" \
        --extra-cflags="-O2 -fPIC" \
        --enable-shared \
        --disable-static \
        --disable-programs \
        --disable-doc \
        --disable-swscale \
        --disable-postproc \
        --disable-avfilter \
        --disable-avdevice \
        --disable-network \
        --disable-everything \
        --enable-swresample \
        --enable-decoder=h264 \
        --enable-decoder=hevc \
        --enable-decoder=av1 \
        --enable-decoder=pcm_s16le \
        --enable-decoder=opus \
        --enable-decoder=aac \
        --enable-decoder=flac \
        --enable-decoder=png \
        --enable-protocol=file \
        --enable-demuxer=image2 \
        --enable-parser=png \
        --enable-zlib \
        --enable-muxer=matroska \
        --enable-muxer=mp4 \
        --enable-muxer=opus \
        --enable-muxer=flac \
        --enable-muxer=wav \
        --disable-vulkan
fi

make -j
make install
