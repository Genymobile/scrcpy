#!/usr/bin/env bash
set -e
DIR=$(dirname ${BASH_SOURCE[0]})
cd "$DIR"
. common
mkdir -p "$PREBUILT_DATA_DIR"
cd "$PREBUILT_DATA_DIR"

DEP_DIR=ffmpeg-win32-4.3.1

FILENAME_SHARED=ffmpeg-4.3.1-win32-shared.zip
SHA256SUM_SHARED=357af9901a456f4dcbacd107e83a934d344c9cb07ddad8aaf80612eeab7d26d2

FILENAME_DEV=ffmpeg-4.3.1-win32-dev.zip
SHA256SUM_DEV=230efb08e9bcf225bd474da29676c70e591fc94d8790a740ca801408fddcb78b

if [[ -d "$DEP_DIR" ]]
then
    echo "$DEP_DIR" found
    exit 0
fi

get_file "https://github.com/Genymobile/scrcpy/releases/download/v1.16/$FILENAME_SHARED" \
    "$FILENAME_SHARED" "$SHA256SUM_SHARED"
get_file "https://github.com/Genymobile/scrcpy/releases/download/v1.16/$FILENAME_DEV" \
    "$FILENAME_DEV" "$SHA256SUM_DEV"

mkdir "$DEP_DIR"
cd "$DEP_DIR"

ZIP_PREFIX_SHARED=ffmpeg-4.3.1-win32-shared
unzip "../$FILENAME_SHARED" \
    "$ZIP_PREFIX_SHARED"/bin/avutil-56.dll \
    "$ZIP_PREFIX_SHARED"/bin/avcodec-58.dll \
    "$ZIP_PREFIX_SHARED"/bin/avformat-58.dll \
    "$ZIP_PREFIX_SHARED"/bin/swresample-3.dll \
    "$ZIP_PREFIX_SHARED"/bin/swscale-5.dll

ZIP_PREFIX_DEV=ffmpeg-4.3.1-win32-dev
unzip "../$FILENAME_DEV" \
    "$ZIP_PREFIX_DEV/include/*"

mv "$ZIP_PREFIX_SHARED"/* .
mv "$ZIP_PREFIX_DEV"/* .
rmdir "$ZIP_PREFIX_SHARED" "$ZIP_PREFIX_DEV"
