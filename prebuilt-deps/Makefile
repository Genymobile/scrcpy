.PHONY: prepare-win32 prepare-win64 \
	prepare-ffmpeg-shared-win32 \
	prepare-ffmpeg-dev-win32 \
	prepare-ffmpeg-shared-win64 \
	prepare-ffmpeg-dev-win64 \
	prepare-libusb \
	prepare-sdl2 \
	prepare-adb

LIBUSB_DIR := libusb-1.0.24

prepare-win32: prepare-sdl2 prepare-ffmpeg-shared-win32 prepare-ffmpeg-dev-win32 prepare-libusb prepare-adb
prepare-win64: prepare-sdl2 prepare-ffmpeg-shared-win64 prepare-ffmpeg-dev-win64 prepare-libusb prepare-adb

prepare-ffmpeg-shared-win32:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win32-shared.zip \
		357af9901a456f4dcbacd107e83a934d344c9cb07ddad8aaf80612eeab7d26d2 \
		ffmpeg-4.3.1-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win32-dev.zip \
		230efb08e9bcf225bd474da29676c70e591fc94d8790a740ca801408fddcb78b \
		ffmpeg-4.3.1-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win64-shared.zip \
		dd29b7f92f48dead4dd940492c7509138c0f99db445076d0a597007298a79940 \
		ffmpeg-4.3.1-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win64-dev.zip \
		2e8038242cf8e1bd095c2978f196ff0462b122cc6ef7e74626a6af15459d8b81 \
		ffmpeg-4.3.1-win64-dev

prepare-libusb:
	# libusb put all things under the root of 7z file, so we pass the last
	# argument which means creating extract dir manually.
	@./prepare-dep https://github.com/libusb/libusb/releases/download/v1.0.24/libusb-1.0.24.7z \
		620cec4dbe4868202949294157da5adb75c9fbb4f04266146fc833eef85f90fb \
		"$(LIBUSB_DIR)" \
		1
	# Our code expects include dir under architechture dir.
	cp -a "$(LIBUSB_DIR)"/include "$(LIBUSB_DIR)"/MinGW32
	cp -a "$(LIBUSB_DIR)"/include "$(LIBUSB_DIR)"/MinGW64

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.16-mingw.tar.gz \
		2bfe48628aa9635c12eac7d421907e291525de1d0b04b3bca4a5bd6e6c881a6f \
		SDL2-2.0.16

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r31.0.3-windows.zip \
		0f4b8fdd26af2c3733539d6eebb3c2ed499ea1d4bb1f4e0ecc2d6016961a6e24 \
		platform-tools
