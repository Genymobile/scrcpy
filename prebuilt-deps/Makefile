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
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.2.1-win32-shared.zip \
		9208255f409410d95147151d7e829b5699bf8d91bfe1e81c3f470f47c2fa66d2 \
		ffmpeg-4.2.1-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.2.1-win32-dev.zip \
		c3469e6c5f031cbcc8cba88dee92d6548c5c6b6ff14f4097f18f72a92d0d70c4 \
		ffmpeg-4.2.1-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.2.1-win64-shared.zip \
		55063d3cf750a75485c7bf196031773d81a1b25d0980c7db48ecfc7701a42331 \
		ffmpeg-4.2.1-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.2.1-win64-dev.zip \
		5af393be5f25c0a71aa29efce768e477c35347f7f8e0d9696767d5b9d405b74e \
		ffmpeg-4.2.1-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.10-mingw.tar.gz \
		a90a7cddaec4996f4d7be6d80c57ec69b062e132bffc513965f99217f603274a \
		SDL2-2.0.10

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r29.0.5-windows.zip \
		2df06160056ec9a84c7334af2a1e42740befbb1a2e34370e7af544a2cc78152c \
		platform-tools
