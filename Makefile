.PHONY: default release clean build build-app build-server run dist dist-zip sums test

GRADLE ?= ./gradlew

APP_BUILD_DIR := app-build
APP_BUILD_DEBUG_DIR := app-build-debug
DIST := dist
TARGET_DIR := scrcpy

VERSION := $(shell git describe --tags --always)
TARGET := $(TARGET_DIR)-$(VERSION).zip

default:
	@echo 'You must specify a target. Try: make release'

release: clean dist-zip sums

clean:
	$(GRADLE) clean
	rm -rf "$(APP_BUILD_DIR)" "$(APP_BUILD_DEBUG_DIR)" "$(DIST)"

build-app-debug:
	[ -d "$(APP_BUILD_DEBUG_DIR)" ] || ( mkdir "$(APP_BUILD_DEBUG_DIR)" && \
		meson app "$(APP_BUILD_DEBUG_DIR)" --buildtype debug )
	ninja -C "$(APP_BUILD_DEBUG_DIR)"

build-server-debug:
	$(GRADLE) assembleDebug

build-debug: build-app-debug build-server-debug

run-debug:
	SCRCPY_SERVER_JAR=server/build/outputs/apk/debug/server-debug.apk $(APP_BUILD_DEBUG_DIR)/scrcpy $(ARGS)

build-app:
	[ -d "$(APP_BUILD_DIR)" ] || ( mkdir "$(APP_BUILD_DIR)" && \
		meson app "$(APP_BUILD_DIR)" --buildtype release -Db_lto )
	ninja -C "$(APP_BUILD_DIR)"

build-server:
	$(GRADLE) assembleRelease

build: build-app build-server

run:
	SCRCPY_SERVER_JAR=server/build/outputs/apk/release/server-release-unsigned.apk $(APP_BUILD_DIR)/scrcpy $(ARGS)

dist: build
	mkdir -p "$(DIST)/$(TARGET_DIR)"
	# no need to sign the APK, we dont "install" it, this is in fact a simple jar
	cp server/build/outputs/apk/release/server-release-unsigned.apk "$(DIST)/$(TARGET_DIR)/scrcpy-server.jar"
	cp $(APP_BUILD_DIR)/scrcpy "$(DIST)/$(TARGET_DIR)/"

dist-zip: dist
	cd "$(DIST)"; \
		zip -r "$(TARGET)" "$(TARGET_DIR)"

sums:
	cd "$(DIST)"; \
		sha256sum *.zip > SHA256SUM.txt

test:
	$(GRADLE) test
	ninja -C "$(APP_BUILD_DIR)" test
