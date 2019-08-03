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
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.1.4-win32-shared.zip \
		596608277f6b937c3dea7c46e854665d75b3de56790bae07f655ca331440f003 \
		ffmpeg-4.1.4-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.1.4-win32-dev.zip \
		a80c86e263cfad26e202edfa5e6e939a2c88843ae26f031d3e0d981a39fd03fb \
		ffmpeg-4.1.4-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.1.4-win64-shared.zip \
		a90889871de2cab8a79b392591313a188189a353f69dde1db98aebe20b280989 \
		ffmpeg-4.1.4-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.1.4-win64-dev.zip \
		6c9d53f9e94ce1821e975ec668e5b9d6e9deb4a45d0d7e30264685d3dfbbb068 \
		ffmpeg-4.1.4-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.10-mingw.tar.gz \
		a90a7cddaec4996f4d7be6d80c57ec69b062e132bffc513965f99217f603274a \
		SDL2-2.0.10

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r29.0.2-windows.zip \
		d78f02e5e2c9c4c1d046dcd4e6fbdf586e5f57ef66eb0da5c2b49d745d85d5ee \
		platform-tools
