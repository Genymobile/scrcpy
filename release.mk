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
	test test-client test-server \
	build-server \
	prepare-deps-win32 prepare-deps-win64 \
	build-win32 build-win64 \
	zip-win32 zip-win64 \
	package release

GRADLE ?= ./gradlew

TEST_BUILD_DIR := build-test
SERVER_BUILD_DIR := build-server
WIN32_BUILD_DIR := build-win32
WIN64_BUILD_DIR := build-win64

VERSION ?= $(shell git describe --tags --exclude='*install-release' --always)

ZIP := zip
WIN32_TARGET_DIR := scrcpy-win32-$(VERSION)
WIN64_TARGET_DIR := scrcpy-win64-$(VERSION)
WIN32_TARGET := $(WIN32_TARGET_DIR).zip
WIN64_TARGET := $(WIN64_TARGET_DIR).zip

RELEASE_DIR := release-$(VERSION)

release: clean test build-server build-win32 build-win64 package

clean:
	$(GRADLE) clean
	rm -rf "$(ZIP)" "$(TEST_BUILD_DIR)" "$(SERVER_BUILD_DIR)" \
		"$(WIN32_BUILD_DIR)" "$(WIN64_BUILD_DIR)"

test-client:
	[ -d "$(TEST_BUILD_DIR)" ] || ( mkdir "$(TEST_BUILD_DIR)" && \
		meson setup "$(TEST_BUILD_DIR)" -Db_sanitize=address )
	ninja -C "$(TEST_BUILD_DIR)"

test-server:
	$(GRADLE) -p server check

test: test-client test-server

build-server:
	$(GRADLE) -p server assembleRelease
	mkdir -p "$(SERVER_BUILD_DIR)/server"
	cp server/build/outputs/apk/release/server-release-unsigned.apk \
		"$(SERVER_BUILD_DIR)/server/scrcpy-server"

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
	# Group intermediate outputs into a 'dist' directory
	mkdir -p "$(WIN32_BUILD_DIR)/dist"
	cp "$(WIN32_BUILD_DIR)"/app/scrcpy.exe "$(WIN32_BUILD_DIR)/dist/"
	cp app/data/scrcpy-console.bat "$(WIN32_BUILD_DIR)/dist/"
	cp app/data/scrcpy-noconsole.vbs "$(WIN32_BUILD_DIR)/dist/"
	cp app/data/icon.png "$(WIN32_BUILD_DIR)/dist/"
	cp app/data/open_a_terminal_here.bat "$(WIN32_BUILD_DIR)/dist/"
	cp app/deps/work/install/win32/bin/*.dll "$(WIN32_BUILD_DIR)/dist/"
	cp app/deps/work/install/win32/bin/adb.exe "$(WIN32_BUILD_DIR)/dist/"

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
	# Group intermediate outputs into a 'dist' directory
	mkdir -p "$(WIN64_BUILD_DIR)/dist"
	cp "$(WIN64_BUILD_DIR)"/app/scrcpy.exe "$(WIN64_BUILD_DIR)/dist/"
	cp app/data/scrcpy-console.bat "$(WIN64_BUILD_DIR)/dist/"
	cp app/data/scrcpy-noconsole.vbs "$(WIN64_BUILD_DIR)/dist/"
	cp app/data/icon.png "$(WIN64_BUILD_DIR)/dist/"
	cp app/data/open_a_terminal_here.bat "$(WIN64_BUILD_DIR)/dist/"
	cp app/deps/work/install/win64/bin/*.dll "$(WIN64_BUILD_DIR)/dist/"
	cp app/deps/work/install/win64/bin/adb.exe "$(WIN64_BUILD_DIR)/dist/"

zip-win32:
	mkdir -p "$(ZIP)/$(WIN32_TARGET_DIR)"
	cp -r "$(WIN32_BUILD_DIR)/dist/." "$(ZIP)/$(WIN32_TARGET_DIR)/"
	cp "$(SERVER_BUILD_DIR)"/server/scrcpy-server "$(ZIP)/$(WIN32_TARGET_DIR)/"
	cd "$(ZIP)"; \
		zip -r "$(WIN32_TARGET)" "$(WIN32_TARGET_DIR)"
	rm -rf "$(ZIP)/$(WIN32_TARGET_DIR)"

zip-win64:
	mkdir -p "$(ZIP)/$(WIN64_TARGET_DIR)"
	cp -r "$(WIN64_BUILD_DIR)/dist/." "$(ZIP)/$(WIN64_TARGET_DIR)/"
	cp "$(SERVER_BUILD_DIR)"/server/scrcpy-server "$(ZIP)/$(WIN64_TARGET_DIR)/"
	cd "$(ZIP)"; \
		zip -r "$(WIN64_TARGET)" "$(WIN64_TARGET_DIR)"
	rm -rf "$(ZIP)/$(WIN64_TARGET_DIR)"

package: zip-win32 zip-win64
	mkdir -p "$(RELEASE_DIR)"
	cp "$(SERVER_BUILD_DIR)/server/scrcpy-server" \
		"$(RELEASE_DIR)/scrcpy-server-$(VERSION)"
	cp "$(ZIP)/$(WIN32_TARGET)" "$(RELEASE_DIR)"
	cp "$(ZIP)/$(WIN64_TARGET)" "$(RELEASE_DIR)"
	cd "$(RELEASE_DIR)" && \
		sha256sum "scrcpy-server-$(VERSION)" \
			"scrcpy-win32-$(VERSION).zip" \
			"scrcpy-win64-$(VERSION).zip" > SHA256SUMS.txt
	@echo "Release generated in $(RELEASE_DIR)/"
