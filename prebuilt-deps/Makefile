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
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.1.3-win32-shared.zip \
		8ea472d673370d5e87517a75587abfa6f189ee4f82e8da21fdbc49d0db0c1a89 \
		ffmpeg-4.1.3-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.1.3-win32-dev.zip \
		e16d3150b6ccf0b71908f5b964cb8c051d79053c8f5cd6d777d617ab4f03613a \
		ffmpeg-4.1.3-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.1.3-win64-shared.zip \
		0b974578e07d974c4bafb36c7ed0b46e46b001d38b149455089c13b57ddefe5d \
		ffmpeg-4.1.3-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.1.3-win64-dev.zip \
		334b473467db096a5b74242743592a73e120a137232794508e4fc55593696a5b \
		ffmpeg-4.1.3-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.9-mingw.tar.gz \
		0f9f00d0f2a9a95dfb5cce929718210c3f85432cc2e9d4abade4adcb7f6bb39d \
		SDL2-2.0.9

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r28.0.1-windows.zip \
		db78f726d5dc653706dcd15a462ab1b946c643f598df76906c4c1858411c54df \
		platform-tools
