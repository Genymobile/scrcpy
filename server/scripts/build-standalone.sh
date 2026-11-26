#!/usr/bin/env bash
# Build the app_process server without Gradle/SDK auto-downloads.
set -euo pipefail
if [[ $# != 3 ]]; then
    printf 'Usage: %s ANDROID_JAR R8_JAR OUTPUT\n' "$0" >&2
    exit 2
fi
android_jar=$(realpath "$1")
r8_jar=$(realpath "$2")
output=$(realpath -m "$3")
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
mkdir -p "$build/classes" "$build/generated/com/genymobile/scrcpy" "$build/dex" "$(dirname "$output")"
aidl="$(dirname "$(dirname "$r8_jar")")/aidl"
framework_aidl="$(dirname "$android_jar")/framework.aidl"
while IFS= read -r -d '' source; do
    "$aidl" -I"$root/server/src/main/aidl" -p"$framework_aidl" -o"$build/generated" "$source"
done < <(python3 - "$root" <<'PY'
import pathlib, sys
for p in sorted((pathlib.Path(sys.argv[1]) / 'server/src/main/aidl').rglob('*.aidl')):
    sys.stdout.buffer.write(str(p).encode() + b'\0')
PY
)
version=$(python3 -c 'import re,sys; print(re.search(r"versionName\s+\"([^\"]+)\"", open(sys.argv[1]).read()).group(1))' "$root/server/build.gradle")
printf 'package com.genymobile.scrcpy; public final class BuildConfig { public static final String VERSION_NAME = "%s"; public static final boolean DEBUG = false; }\n' "$version" > "$build/generated/com/genymobile/scrcpy/BuildConfig.java"
python3 - "$root" "$build" <<'PY'
import pathlib, sys
root, build = map(pathlib.Path, sys.argv[1:])
paths = sorted((root / 'server/src/main/java').rglob('*.java'))
paths += sorted((build / 'generated').rglob('*.java'))
(build / 'sources.txt').write_text(''.join('"' + str(p) + '"\n' for p in paths))
PY
javac -encoding UTF-8 --release 8 -classpath "$android_jar" -d "$build/classes" @"$build/sources.txt"
jar --create --file "$build/classes.jar" -C "$build/classes" .
java -cp "$r8_jar" com.android.tools.r8.D8 --release --min-api 21 --lib "$android_jar" --output "$build/dex" "$build/classes.jar"
# app_process loads classes.dex directly from the zip; no APK manifest/resources/signature required.
revision=$(git -C "$root" rev-parse HEAD)
printf 'source_revision=%s\nversion=%s\n' "$revision" "$version" > "$build/dex/build-info.txt"
jar --create --file "$output" --no-manifest -C "$build/dex" .
