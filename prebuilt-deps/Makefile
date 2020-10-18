.PHONY: prepare-win32 prepare-win64 \
	prepare-ffmpeg-shared-win32 \
	prepare-ffmpeg-dev-win32 \
	prepare-ffmpeg-shared-win64 \
	prepare-ffmpeg-dev-win64 \
	prepare-sdl2 \
	prepare-adb

prepare-win32: prepare-sdl2 prepare-ffmpeg-shared-win32 prepare-ffmpeg-dev-win32 prepare-adb
prepare-win64: prepare-sdl2 prepare-ffmpeg-shared-win64 prepare-ffmpeg-dev-win64 prepare-adb

prepare-ffmpeg-shared-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.3.1-win32-shared.zip \
		357af9901a456f4dcbacd107e83a934d344c9cb07ddad8aaf80612eeab7d26d2 \
		ffmpeg-4.3.1-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.3.1-win32-dev.zip \
		230efb08e9bcf225bd474da29676c70e591fc94d8790a740ca801408fddcb78b \
		ffmpeg-4.3.1-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2020-10-17-12-30/ffmpeg-n4.3.1-20-g8a2acdc6da-win64-lgpl-shared-4.3.zip \
		db3a355a2569044ceb7b417aae0b4bed4a01b1ef0a762dd7a4610bd34d886203 \
		ffmpeg-4.3.1-win64-shared

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.12-mingw.tar.gz \
		e614a60f797e35ef9f3f96aef3dc6a1d786de3cc7ca6216f97e435c0b6aafc46 \
		SDL2-2.0.12

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r30.0.4-windows.zip \
		413182fff6c5957911e231b9e97e6be4fc6a539035e3dfb580b5c54bd5950fee \
		platform-tools
