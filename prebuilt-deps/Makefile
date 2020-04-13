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
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.10-mingw.tar.gz \
		a90a7cddaec4996f4d7be6d80c57ec69b062e132bffc513965f99217f603274a \
		SDL2-2.0.10

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r29.0.5-windows.zip \
		2df06160056ec9a84c7334af2a1e42740befbb1a2e34370e7af544a2cc78152c \
		platform-tools
