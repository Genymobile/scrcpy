.PHONY: default release clean build-app build-server dist dist-zip sums test

BUILD_DIR := build
DIST := dist
TARGET_DIR := scrcpy

VERSION := $(shell git describe --tags --always)
TARGET := $(TARGET_DIR)-$(VERSION).zip

default:
	@echo 'You must specify a target. Try: make release'

release: clean dist-zip sums

clean:
	rm -rf "$(BUILD_DIR)" "$(DIST)"
	+$(MAKE) -C server clean

build-app:
	[ -d "$(BUILD_DIR)" ] || ( mkdir "$(BUILD_DIR)" && meson app "$(BUILD_DIR)" --buildtype release )
	ninja -C "$(BUILD_DIR)"

build-server:
	+$(MAKE) -C server clean
	+$(MAKE) -C server jar

dist: build-app build-server
	mkdir -p "$(DIST)/$(TARGET_DIR)"
	cp server/scrcpy-server.jar "$(DIST)/$(TARGET_DIR)/"
	cp build/scrcpy "$(DIST)/$(TARGET_DIR)/"

dist-zip: dist
	cd "$(DIST)"; \
		zip -r "$(TARGET)" "$(TARGET_DIR)"

sums:
	cd "$(DIST)"; \
		sha256sum *.zip > SHA256SUM.txt

test:
	+$(MAKE) -C server test
	ninja -C "$(BUILD_DIR)" test
