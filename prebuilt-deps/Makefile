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
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.0-win32-shared.zip \
		530c92df0ca14c35901b4b681847d62da3c50a0cc9b7ced37b04968f6b5c243d \
		ffmpeg-4.0-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.0-win32-dev.zip \
		e2f5200b5e73c4d0abb9b89c4ffc0438f92a0aadc54c81cf57e18c81a9f11c6b \
		ffmpeg-4.0-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.0-win64-shared.zip \
		8fe2d344463dbefc2db4239a4203a55ed0324faceaae57276a40c4fabda84c37 \
		ffmpeg-4.0-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.0-win64-dev.zip \
		facced738eabfc53fa92834dea8b24426f64db61298688fed480145945be07fa \
		ffmpeg-4.0-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.8-mingw.tar.gz \
		ffff7305d634aff5e1df5b7bb935435c3a02c8b03ad94a1a2be9169a558a7961 \
		SDL2-2.0.8

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r27.0.1-windows.zip \
	    880662adfb0d6911ff250b9e13930ae1a4110fc36d5866afd4f8f56d935f7939 \
		platform-tools
