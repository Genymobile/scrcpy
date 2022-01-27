.PHONY: prepare-win32 prepare-win64 \
	prepare-ffmpeg-win32 \
	prepare-ffmpeg-win64 \
	prepare-sdl2 \
	prepare-adb

prepare-win32: prepare-sdl2 prepare-ffmpeg-win32 prepare-adb
prepare-win64: prepare-sdl2 prepare-ffmpeg-win64 prepare-adb

# Use old FFmpeg version for win32, there are no new prebuilts
prepare-ffmpeg-win32:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win32-shared.zip \
		357af9901a456f4dcbacd107e83a934d344c9cb07ddad8aaf80612eeab7d26d2 \
		ffmpeg-4.3.1-win32-shared
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win32-dev.zip \
		230efb08e9bcf225bd474da29676c70e591fc94d8790a740ca801408fddcb78b \
		ffmpeg-4.3.1-win32-dev
	ln -sf ../ffmpeg-4.3.1-win32-dev/include ffmpeg-4.3.1-win32-shared/

prepare-ffmpeg-win64:
	@./prepare-dep https://github.com/GyanD/codexffmpeg/releases/download/5.0/ffmpeg-5.0-full_build-shared.7z \
		e5900f6cecd4c438d398bd2fc308736c10b857cd8dd61c11bcfb05bff5d1211a \
		ffmpeg-5.0-full_build-shared

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.20-mingw.tar.gz \
		38094d82a857d6c62352e5c5cdec74948c5b4d25c59cbd298d6d233568976bd1 \
		SDL2-2.0.20

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r32.0.0-windows.zip \
		41f4c7512b32cbb3f8c624c20b56326abb692a6f169b03b4b63b6c5a6fdbb08c \
		platform-tools
