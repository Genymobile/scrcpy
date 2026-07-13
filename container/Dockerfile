# Build with:
#     podman build -t scrcpy-builder .
#
# Run with:
#     podman run \
#       -v PATH_TO_SCRCPY:/home/debian/scrcpy \
#       --userns=keep-id \
#       -it scrcpy-builder \
#       bash

FROM debian:trixie

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    sudo wget unzip gcc git pkg-config meson ninja-build cmake \
    libsdl3-dev libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev \
    libswresample-dev libusb-1.0-0-dev openjdk-21-jdk \
    mingw-w64 mingw-w64-tools libz-mingw-w64-dev

RUN groupadd -g 1000 debian \
 && useradd -m -u 1000 -g 1000 -s /bin/bash debian
RUN echo "debian ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

ENV ANDROID_HOME=/opt/android/sdk
RUN mkdir -p "$ANDROID_HOME" \
 && chown debian:debian "$ANDROID_HOME"

USER debian
WORKDIR /home/debian

ENV CMDLINETOOLS_URL=https://dl.google.com/android/repository/commandlinetools-linux-13114758_latest.zip
ENV CMDLINETOOLS_SHA256=7ec965280a073311c339e571cd5de778b9975026cfcbe79f2b1cdcb1e15317ee

RUN wget -q "$CMDLINETOOLS_URL" -O cmdlinetools.zip \
 && echo "$CMDLINETOOLS_SHA256  cmdlinetools.zip" | sha256sum -c

RUN mkdir tmp \
 && unzip -q cmdlinetools.zip -d tmp \
 && mkdir -p "$ANDROID_HOME/cmdline-tools" \
 && mv tmp/cmdline-tools "$ANDROID_HOME/cmdline-tools/latest" \
 && rmdir tmp \
 && rm cmdlinetools.zip
# To get the licence hash, run manually: "$ANDROID_HOME"/cmdline-tools/latest/bin/sdkmanager --licenses
# Build-tools 35 is still needed for the current AGP version
RUN mkdir -p "$ANDROID_HOME/licenses" \
 && echo 24333f8a63b6825ea9c5514f83c2829b004d1fee > "$ANDROID_HOME/licenses/android-sdk-license" \
 && $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager "platform-tools" "platforms;android-36" "build-tools;35.0.0" "build-tools;36.0.0"

# For scrcpy build scripts
ENV GRADLE_VERSION=8.14.3
ENV GRADLE_HOME=/opt/gradle-$GRADLE_VERSION
ENV GRADLE_URL=https://services.gradle.org/distributions/gradle-$GRADLE_VERSION-bin.zip
ENV GRADLE_SHA256=bd71102213493060956ec229d946beee57158dbd89d0e62b91bca0fa2c5f3531
ENV GRADLE=gradle
ENV PATH=$GRADLE_HOME/bin:$PATH

USER root
RUN wget -q "$GRADLE_URL" -O gradle.zip \
 && echo "$GRADLE_SHA256  gradle.zip" | sha256sum -c
RUN unzip -q gradle.zip -d /opt \
 && rm gradle.zip

USER debian

# Pre-download gradle dependencies for scrcpy
RUN mkdir -p /home/debian/fake-scrcpy/app
COPY fake.gradle /home/debian/fake-scrcpy/build.gradle
COPY fake_app.gradle /home/debian/fake-scrcpy/app/build.gradle
RUN echo "include ':app'" > /home/debian/fake-scrcpy/settings.gradle \
 && gradle -p /home/debian/fake-scrcpy dependencies androidDependencies --no-daemon \
 && rm -rf /home/debian/fake-scrcpy
