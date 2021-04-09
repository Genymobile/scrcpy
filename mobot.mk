# Mobot makefile
# Build the Mobot version of scrcpy with Mobot's FFmpeg
#  make -f mobot.mk

.DEFAULT_GOAL := scrcpy

build-ffmpeg:
	git clone https://github.com/team-mobot/FFmpeg.git build-ffmpeg

AVLIBS:=build-libav/lib/libavformat.a build-libav/lib/libavcodec.a build-libav/lib/libavutil.a build-libav/lib/libswscale.a
$(AVLIBS): build-ffmpeg
	# Build Mobot version of FFmpeg and install in subdir libav
	cd build-ffmpeg && \
	git checkout encode-png-metadata && \
	./configure --prefix="../build-libav" \
				--pkg-config-flags="--static" \
				--extra-libs="-lpthread -lm" \
				--enable-gpl --enable-nonfree \
				--disable-bsfs --disable-filters \
				--disable-encoders --enable-encoder=png \
				--disable-decoders --enable-decoder=h264 \
				--enable-libx264 && \
	make install-libs install-headers

build-app:
	meson build-app --buildtype release --strip -Db_lto=true \
		-Dlocal_libav=${PWD}/build-libav/lib \
		-Dprebuilt_server=/usr/local/share/scrcpy/scrcpy-server

scrcpy: build-app $(AVLIBS)
	ninja -Cbuild-app
	cp build-app/app/scrcpy .
