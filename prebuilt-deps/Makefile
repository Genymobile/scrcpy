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
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win32-shared.zip \
		357af9901a456f4dcbacd107e83a934d344c9cb07ddad8aaf80612eeab7d26d2 \
		ffmpeg-4.3.1-win32-shared

prepare-ffmpeg-dev-win32:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win32-dev.zip \
		230efb08e9bcf225bd474da29676c70e591fc94d8790a740ca801408fddcb78b \
		ffmpeg-4.3.1-win32-dev

prepare-ffmpeg-shared-win64:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win64-shared.zip \
		dd29b7f92f48dead4dd940492c7509138c0f99db445076d0a597007298a79940 \
		ffmpeg-4.3.1-win64-shared

prepare-ffmpeg-dev-win64:
	@./prepare-dep https://github.com/Genymobile/scrcpy/releases/download/v1.16/ffmpeg-4.3.1-win64-dev.zip \
		2e8038242cf8e1bd095c2978f196ff0462b122cc6ef7e74626a6af15459d8b81 \
		ffmpeg-4.3.1-win64-dev

prepare-sdl2:
	@./prepare-dep https://libsdl.org/release/SDL2-devel-2.0.12-mingw.tar.gz \
		e614a60f797e35ef9f3f96aef3dc6a1d786de3cc7ca6216f97e435c0b6aafc46 \
		SDL2-2.0.12

prepare-adb:
	@./prepare-dep https://dl.google.com/android/repository/platform-tools_r30.0.5-windows.zip \
		549ba2bdc31f335eb8a504f005f77606a479cc216d6b64a3e8b64c780003661f \
		platform-tools
