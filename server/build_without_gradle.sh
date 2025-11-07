#!/usr/bin/env bash
#
# This script generates the scrcpy binary "manually" (without gradle).
#
# Adapt Android platform and build tools versions (via ANDROID_PLATFORM and
# ANDROID_BUILD_TOOLS environment variables).
#
# Then execute:
#
#     BUILD_DIR=my_build_dir ./build_without_gradle.sh

set -e

SCRCPY_DEBUG=false
SCRCPY_VERSION_NAME=3.3.3

PLATFORM=${ANDROID_PLATFORM:-35}
BUILD_TOOLS=${ANDROID_BUILD_TOOLS:-35.0.0}
PLATFORM_TOOLS="$ANDROID_HOME/platforms/android-$PLATFORM"
BUILD_TOOLS_DIR="$ANDROID_HOME/build-tools/$BUILD_TOOLS"

BUILD_DIR="$(realpath ${BUILD_DIR:-build_manual})"
CLASSES_DIR="$BUILD_DIR/classes"
GEN_DIR="$BUILD_DIR/gen"
SERVER_DIR=$(dirname "$0")
SERVER_BINARY=scrcpy-server
ANDROID_JAR="$PLATFORM_TOOLS/android.jar"
ANDROID_AIDL="$PLATFORM_TOOLS/framework.aidl"
LAMBDA_JAR="$BUILD_TOOLS_DIR/core-lambda-stubs.jar"

echo "Platform: android-$PLATFORM"
echo "Build-tools: $BUILD_TOOLS"
echo "Build dir: $BUILD_DIR"

rm -rf "$CLASSES_DIR" "$GEN_DIR" "$BUILD_DIR/$SERVER_BINARY" classes.dex
mkdir -p "$CLASSES_DIR"
mkdir -p "$GEN_DIR/com/genymobile/scrcpy"

<< EOF cat > "$GEN_DIR/com/genymobile/scrcpy/BuildConfig.java"
package com.genymobile.scrcpy;

public final class BuildConfig {
  public static final boolean DEBUG = $SCRCPY_DEBUG;
  public static final String VERSION_NAME = "$SCRCPY_VERSION_NAME";
}
EOF

echo "Generating java from aidl..."
cd "$SERVER_DIR/src/main/aidl"
"$BUILD_TOOLS_DIR/aidl" -o"$GEN_DIR" -I. \
    android/content/IOnPrimaryClipChangedListener.aidl
"$BUILD_TOOLS_DIR/aidl" -o"$GEN_DIR" -I. -p "$ANDROID_AIDL" \
    android/view/IDisplayWindowListener.aidl

# Fake sources to expose hidden Android types to the project
FAKE_SRC=( \
    android/content/*java \
)

SRC=( \
    com/genymobile/scrcpy/*.java \
    com/genymobile/scrcpy/audio/*.java \
    com/genymobile/scrcpy/control/*.java \
    com/genymobile/scrcpy/device/*.java \
    com/genymobile/scrcpy/opengl/*.java \
    com/genymobile/scrcpy/util/*.java \
    com/genymobile/scrcpy/video/*.java \
    com/genymobile/scrcpy/wrappers/*.java \
)

CLASSES=()
for src in "${SRC[@]}"
do
    CLASSES+=("${src%.java}.class")
done

echo "Compiling java sources..."
cd ../java
javac -encoding UTF-8 -bootclasspath "$ANDROID_JAR" \
    -cp "$LAMBDA_JAR:$GEN_DIR" \
    -d "$CLASSES_DIR" \
    -source 1.8 -target 1.8 \
    ${FAKE_SRC[@]} \
    ${SRC[@]}

echo "Dexing..."
cd "$CLASSES_DIR"

if [[ $PLATFORM -lt 31 ]]
then
    # use dx
    "$BUILD_TOOLS_DIR/dx" --dex --output "$BUILD_DIR/classes.dex" \
        android/view/*.class \
        android/content/*.class \
        ${CLASSES[@]}

    echo "Archiving..."
    cd "$BUILD_DIR"
    jar cvf "$SERVER_BINARY" classes.dex
    rm -rf classes.dex
else
    # use d8
    "$BUILD_TOOLS_DIR/d8" --classpath "$ANDROID_JAR" \
        --output "$BUILD_DIR/classes.zip" \
        android/view/*.class \
        android/content/*.class \
        ${CLASSES[@]}

    cd "$BUILD_DIR"
    mv classes.zip "$SERVER_BINARY"
fi

rm -rf "$GEN_DIR" "$CLASSES_DIR"

echo "Server generated in $BUILD_DIR/$SERVER_BINARY"
