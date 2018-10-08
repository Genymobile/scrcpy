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
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-4.0.2-win32-shared.zip \
		cc190a3a4cf7bfbd4fbaa92609c1501a1de458055e6cfea8b745c1d515013aa8 \
		ffmpeg-4.0.2-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-4.0.2-win32-dev.zip \
		c72c74bad74ac0541f1b43090c26a50017c49041c182a703abd2057bb8cdc238 \
		ffmpeg-4.0.2-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/shared/ffmpeg-4.0.2-win64-shared.zip \
		ede566aca8b5348dff85570f9638c6bad33209f9419f79db7bde7daa37599bff \
		ffmpeg-4.0.2-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://ffmpeg.zeranoe.com/builds/win64/dev/ffmpeg-4.0.2-win64-dev.zip \
		23ee994161c51285cb956b98d3caa499d48083dae7b26c1fdf77f22e98df1c5f \
		ffmpeg-4.0.2-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.8-mingw.tar.gz \
		ffff7305d634aff5e1df5b7bb935435c3a02c8b03ad94a1a2be9169a558a7961 \
		SDL2-2.0.8

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r28.0.1-windows.zip \
		db78f726d5dc653706dcd15a462ab1b946c643f598df76906c4c1858411c54df \
		platform-tools
