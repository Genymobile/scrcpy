#!/usr/bin/env bash
. init_deps

VERSION=5.0.1
FILENAME=ffmpeg-$VERSION.tar.xz

URL=http://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz
SHA256SUM=ef2efae259ce80a240de48ec85ecb062cecca26e4352ffb3fda562c21a93007b

DEP_DIR="$DATA_DIR/ffmpeg-$VERSION-$SHA256SUM"

if [[ ! -d "$DEP_DIR" ]]
then
    get_file "$URL" "$FILENAME" "$SHA256SUM"

    mkdir "$DEP_DIR"
    cd "$DEP_DIR"

    tar xvf "../$FILENAME"
else
    echo "$DEP_DIR found"
    cd "$DEP_DIR"
fi

cd "ffmpeg-$VERSION"
rm -rf "build-$HOST"
mkdir "build-$HOST"
cd "build-$HOST"

params=(
    --prefix="$INSTALL_DIR"
    --disable-autodetect
    --disable-programs
    --disable-everything
    --disable-doc
    --disable-swresample
    --disable-swscale
    --disable-avfilter
    --disable-postproc
    --disable-static
    --enable-shared
    --enable-decoder=h264
    --enable-decoder=png
    --enable-muxer=mp4
    --enable-muxer=matroska
)
if [[ "$HOST_SYSTEM" == 'linux' ]]
then
    params+=(--enable-libv4l2)
fi

../configure "${params[@]}"
make -j $NJOBS
make install
