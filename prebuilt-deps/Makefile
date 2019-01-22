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
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.1-win32-shared.zip \
		e692b18c01745d262c03294b382fd64df68fabe3c66aa4546a3ad3935175cde3 \
		ffmpeg-4.1-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.1-win32-dev.zip \
		34bc5e471fb9160609abd6bc271e361050f3ff7376b1b8a0873cca02b38277c8 \
		ffmpeg-4.1-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.1-win64-shared.zip \
		c4908c97436c946509dc365e421159274fa4b1e66dce6fb5b63d82a6294d5357 \
		ffmpeg-4.1-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.1-win64-dev.zip \
		761ec79aa3dae66698c9791a2f0bb9da8794246f8356cadc741ddc0eabab0471 \
		ffmpeg-4.1-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.9-mingw.tar.gz \
		0f9f00d0f2a9a95dfb5cce929718210c3f85432cc2e9d4abade4adcb7f6bb39d \
		SDL2-2.0.9

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r28.0.1-windows.zip \
		db78f726d5dc653706dcd15a462ab1b946c643f598df76906c4c1858411c54df \
		platform-tools
