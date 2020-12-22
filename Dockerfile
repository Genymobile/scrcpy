FROM thyrlian/android-sdk:latest

# Set Timezone and Install Build Dependencies
RUN export TZ="Etc/UTC"; ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
   && apt-get update && apt-get -y install ffmpeg libsdl2-2.0-0 android-sdk adb build-essential git pkg-config meson ninja-build libavcodec-dev libavformat-dev libavutil-dev libsdl2-dev openjdk-8-jdk python3-pip mingw-w64 mingw-w64-tools android-tools-adb zip

# Add builder user so that build is not running as root
RUN groupadd -r builder && useradd -m -r -g builder builder && chown -R builder:builder /opt/android-sdk
USER builder

CMD cd /scrcpy && ./release.sh && cd /scrcpy && meson x --buildtype release --strip -Db_lto=true && ninja -Cx
