# This makefile provides recipes to build a "portable" version of scrcpy.
#
# Here, "portable" means that the client and server binaries are expected to be
# anywhere, but in the same directory, instead of well-defined separate
# locations (e.g. /usr/bin/scrcpy and /usr/share/scrcpy/scrcpy-server.jar).
#
# In particular, this implies to change the location from where the client push
# the server to the device.
#
# "make release-portable" builds a zip containing the client and the server.
#
# This is a simple Makefile because Meson is not flexible enough to execute some
# arbitrary commands.

.PHONY: default clean build-portable release-portable dist-portable dist-portable-zip sums test check

GRADLE ?= ./gradlew

PORTABLE_BUILD_DIR := build-portable
DIST := dist
TARGET_DIR := scrcpy

VERSION := $(shell git describe --tags --always)
TARGET := $(TARGET_DIR)-$(VERSION).zip

default:
	@echo 'You must specify a target. Try: make release-portable'

clean:
	$(GRADLE) clean
	rm -rf "$(PORTABLE_BUILD_DIR)" "$(DIST)"

build-portable:
	[ -d "$(PORTABLE_BUILD_DIR)" ] || ( mkdir "$(PORTABLE_BUILD_DIR)" && \
		meson "$(PORTABLE_BUILD_DIR)" \
			--buildtype release --strip -Db_lto=true \
			-Doverride_server_path=scrcpy-server.jar )
	ninja -C "$(PORTABLE_BUILD_DIR)"

release-portable: clean dist-portable-zip sums
	@echo "Release created in $(DIST)/."

dist-portable: build-portable
	mkdir -p "$(DIST)/$(TARGET_DIR)"
	cp "$(PORTABLE_BUILD_DIR)"/server/scrcpy-server.jar "$(DIST)/$(TARGET_DIR)/"
	cp "$(PORTABLE_BUILD_DIR)"/app/scrcpy "$(DIST)/$(TARGET_DIR)/"

dist-portable-zip: dist-portable
	cd "$(DIST)"; \
		zip -r "$(TARGET)" "$(TARGET_DIR)"

sums:
	cd "$(DIST)"; \
		sha256sum *.zip > SHA256SUM.txt

test: build-portable
	$(GRADLE) test
	ninja -C "$(PORTABLE_BUILD_DIR)" test

check: build-portable
	$(GRADLE) check
	ninja -C "$(PORTABLE_BUILD_DIR)" test
