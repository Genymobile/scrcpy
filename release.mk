# This makefile provides recipes to build a "portable" version of scrcpy for
# Windows.
#
# Here, "portable" means that the client and server binaries are expected to be
# anywhere, but in the same directory, instead of well-defined separate
# locations (e.g. /usr/bin/scrcpy and /usr/share/scrcpy/scrcpy-server).
#
# In particular, this implies to change the location from where the client push
# the server to the device.

.PHONY: default clean \
	test \
	build-server \
	prepare-deps \
	build-win32 build-win64 \
	dist-win32 dist-win64 \
	zip-win32 zip-win64 \
	release

GRADLE ?= ./gradlew

TEST_BUILD_DIR := build-test
SERVER_BUILD_DIR := build-server
WIN32_BUILD_DIR := build-win32
WIN64_BUILD_DIR := build-win64

VERSION := $(shell git describe --tags --always)

DIST := dist
WIN32_TARGET_DIR := scrcpy-win32-$(VERSION)
WIN64_TARGET_DIR := scrcpy-win64-$(VERSION)
WIN32_TARGET := $(WIN32_TARGET_DIR).zip
WIN64_TARGET := $(WIN64_TARGET_DIR).zip

RELEASE_DIR := release-$(VERSION)

release: clean test build-server zip-win32 zip-win64
	mkdir -p "$(RELEASE_DIR)"
	cp "$(SERVER_BUILD_DIR)/server/scrcpy-server" \
		"$(RELEASE_DIR)/scrcpy-server-$(VERSION)"
	cp "$(DIST)/$(WIN32_TARGET)" "$(RELEASE_DIR)"
	cp "$(DIST)/$(WIN64_TARGET)" "$(RELEASE_DIR)"
	cd "$(RELEASE_DIR)" && \
		sha256sum "scrcpy-server-$(VERSION)" \
			"scrcpy-win32-$(VERSION).zip" \
			"scrcpy-win64-$(VERSION).zip" > SHA256SUMS.txt
	@echo "Release generated in $(RELEASE_DIR)/"

clean:
	$(GRADLE) clean
	rm -rf "$(DIST)" "$(TEST_BUILD_DIR)" "$(SERVER_BUILD_DIR)" \
		"$(WIN32_BUILD_DIR)" "$(WIN64_BUILD_DIR)"

test:
	[ -d "$(TEST_BUILD_DIR)" ] || ( mkdir "$(TEST_BUILD_DIR)" && \
		meson setup "$(TEST_BUILD_DIR)" -Db_sanitize=address )
	ninja -C "$(TEST_BUILD_DIR)"
	$(GRADLE) -p server check

build-server:
	[ -d "$(SERVER_BUILD_DIR)" ] || ( mkdir "$(SERVER_BUILD_DIR)" && \
		meson setup "$(SERVER_BUILD_DIR)" --buildtype release -Dcompile_app=false )
	ninja -C "$(SERVER_BUILD_DIR)"

prepare-deps-win32:
	@app/deps/adb.sh win32
	@app/deps/sdl.sh win32
	@app/deps/ffmpeg.sh win32
	@app/deps/libusb.sh win32

prepare-deps-win64:
	@app/deps/adb.sh win64
	@app/deps/sdl.sh win64
	@app/deps/ffmpeg.sh win64
	@app/deps/libusb.sh win64

build-win32: prepare-deps-win32
	rm -rf "$(WIN32_BUILD_DIR)"
	mkdir -p "$(WIN32_BUILD_DIR)/local"
	meson setup "$(WIN32_BUILD_DIR)" \
		--pkg-config-path="app/deps/work/install/win32/lib/pkgconfig" \
		-Dc_args="-I$(PWD)/app/deps/work/install/win32/include" \
		-Dc_link_args="-L$(PWD)/app/deps/work/install/win32/lib" \
		--cross-file=cross_win32.txt \
		--buildtype=release --strip -Db_lto=true \
		-Dcompile_server=false \
		-Dportable=true
	ninja -C "$(WIN32_BUILD_DIR)"

build-win64: prepare-deps-win64
	rm -rf "$(WIN64_BUILD_DIR)"
	mkdir -p "$(WIN64_BUILD_DIR)/local"
	meson setup "$(WIN64_BUILD_DIR)" \
		--pkg-config-path="app/deps/work/install/win64/lib/pkgconfig" \
		-Dc_args="-I$(PWD)/app/deps/work/install/win64/include" \
		-Dc_link_args="-L$(PWD)/app/deps/work/install/win64/lib" \
		--cross-file=cross_win64.txt \
		--buildtype=release --strip -Db_lto=true \
		-Dcompile_server=false \
		-Dportable=true
	ninja -C "$(WIN64_BUILD_DIR)"

dist-win32: build-server build-win32
	mkdir -p "$(DIST)/$(WIN32_TARGET_DIR)"
	cp "$(SERVER_BUILD_DIR)"/server/scrcpy-server "$(DIST)/$(WIN32_TARGET_DIR)/"
	cp "$(WIN32_BUILD_DIR)"/app/scrcpy.exe "$(DIST)/$(WIN32_TARGET_DIR)/"
	cp app/data/scrcpy-console.bat "$(DIST)/$(WIN32_TARGET_DIR)/"
	cp app/data/scrcpy-noconsole.vbs "$(DIST)/$(WIN32_TARGET_DIR)/"
	cp app/data/icon.png "$(DIST)/$(WIN32_TARGET_DIR)/"
	cp app/data/open_a_terminal_here.bat "$(DIST)/$(WIN32_TARGET_DIR)/"
	cp app/deps/work/install/win32/bin/*.dll "$(DIST)/$(WIN32_TARGET_DIR)/"
	cp app/deps/work/install/win32/bin/adb.exe "$(DIST)/$(WIN32_TARGET_DIR)/"

dist-win64: build-server build-win64
	mkdir -p "$(DIST)/$(WIN64_TARGET_DIR)"
	cp "$(SERVER_BUILD_DIR)"/server/scrcpy-server "$(DIST)/$(WIN64_TARGET_DIR)/"
	cp "$(WIN64_BUILD_DIR)"/app/scrcpy.exe "$(DIST)/$(WIN64_TARGET_DIR)/"
	cp app/data/scrcpy-console.bat "$(DIST)/$(WIN64_TARGET_DIR)/"
	cp app/data/scrcpy-noconsole.vbs "$(DIST)/$(WIN64_TARGET_DIR)/"
	cp app/data/icon.png "$(DIST)/$(WIN64_TARGET_DIR)/"
	cp app/data/open_a_terminal_here.bat "$(DIST)/$(WIN64_TARGET_DIR)/"
	cp app/deps/work/install/win64/bin/*.dll "$(DIST)/$(WIN64_TARGET_DIR)/"
	cp app/deps/work/install/win64/bin/adb.exe "$(DIST)/$(WIN64_TARGET_DIR)/"

zip-win32: dist-win32
	cd "$(DIST)"; \
		zip -r "$(WIN32_TARGET)" "$(WIN32_TARGET_DIR)"

zip-win64: dist-win64
	cd "$(DIST)"; \
		zip -r "$(WIN64_TARGET)" "$(WIN64_TARGET_DIR)"
