# Mobot makefile
# Build the Mobot version of scrcpy with Mobot's FFmpeg
#  make -f mobot.mk
# On Linux it may be necessary to install dependencies
#  sudo apt-get install cmake ninja libpng-dev libsdl2-dev
#  pip3 install meson
#  sudo snap install scrcpy (for server .jar)
# On Raspberry Pi it may be necessary to install
#  sudo apt-get install ninja-build meson libudev1 libsdl2-dev

.DEFAULT_GOAL := scrcpy

AVDIR:=build-libav
AVLIBDIR:=$(AVDIR)/lib
AVLIBS:=$(AVLIBDIR)/libavformat.a $(AVLIBDIR)/libavcodec.a $(AVLIBDIR)/libavutil.a $(AVLIBDIR)/libswscale.a

build-ffmpeg:
	git clone https://github.com/team-mobot/FFmpeg.git build-ffmpeg

$(AVLIBS): build-ffmpeg
	# Build Mobot version of FFmpeg and install in subdir libav
	cd build-ffmpeg && \
	git checkout release/4.3 && \
	./configure --prefix="../build-libav" \
				--pkg-config-flags="--static" \
				--enable-gpl --enable-nonfree \
				--disable-bsfs --disable-filters \
				--disable-encoders --enable-encoder=png \
				--disable-decoders --enable-decoder=h264 \
				--enable-libx264 && \
	make install-libs install-headers

build-app: $(AVLIBS)
	LDFLAGS="-Wl,-lm -Wl,-lpthread" \
		meson build-app --buildtype release --strip -Db_lto=true \
		-Dlocal_libav=$(AVDIR) \
		-Dcompile_server=false \
		-Dportable=true \
		|| (ret=$$?; rm -rf $@ && exit $$ret)

scrcpy: build-app
	ninja -Cbuild-app
