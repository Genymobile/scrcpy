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
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.2.2-win32-shared.zip \
		ab5d603aaa54de360db2c2ffe378c82376b9343ea1175421dd644639aa07ee31 \
		ffmpeg-4.2.2-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.2.2-win32-dev.zip \
		8d224be567a2950cad4be86f4aabdd045bfa52ad758e87c72cedd278613bc6c8 \
		ffmpeg-4.2.2-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.2.2-win64-shared.zip \
		5aedf268952b7d9f6541dbfcb47cd86a7e7881a3b7ba482fd3bc4ca33bda7bf5 \
		ffmpeg-4.2.2-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.2.2-win64-dev.zip \
		f4885f859c5b0d6663c2a0a4c1cf035b1c60b146402790b796bd3ad84f4f3ca2 \
		ffmpeg-4.2.2-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.12-mingw.tar.gz \
		e614a60f797e35ef9f3f96aef3dc6a1d786de3cc7ca6216f97e435c0b6aafc46 \
		SDL2-2.0.12

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r30.0.0-windows.zip \
		854305f9a702f5ea2c3de73edde402bd26afa0ee944c9b0c4380420f5a862e0d \
		platform-tools
