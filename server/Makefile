.PHONY: jar push run clean compile compiletests test

SRC_DIR := src
GEN_DIR := gen
CLS_DIR := classes
CLS_DEX := classes.dex
TEST_SRC_DIR := tests
TEST_CLS_DIR := test_classes
TEST_LIBS := /usr/share/java/junit4.jar:/usr/share/java/hamcrest-core.jar

BUILD_TOOLS := $(ANDROID_HOME)/build-tools/26.0.2
AIDL := $(BUILD_TOOLS)/aidl
ifeq ($(OS),Windows_NT)
	DX := $(BUILD_TOOLS)/dx.bat
else
	DX := $(BUILD_TOOLS)/dx
endif

ANDROID_JAR := $(ANDROID_HOME)/platforms/android-26/android.jar

AIDL_SRC := android/view/IRotationWatcher.aidl
SRC := com/genymobile/scrcpy/ScrCpyServer.java \
	com/genymobile/scrcpy/ControlEvent.java \
	com/genymobile/scrcpy/ControlEventReader.java \
	com/genymobile/scrcpy/DesktopConnection.java \
	com/genymobile/scrcpy/Device.java \
	com/genymobile/scrcpy/DisplayInfo.java \
	com/genymobile/scrcpy/EventController.java \
	com/genymobile/scrcpy/Ln.java \
	com/genymobile/scrcpy/Options.java \
	com/genymobile/scrcpy/Point.java \
	com/genymobile/scrcpy/Position.java \
	com/genymobile/scrcpy/ScreenInfo.java \
	com/genymobile/scrcpy/ScreenStreamer.java \
	com/genymobile/scrcpy/ScreenStreamerSession.java \
	com/genymobile/scrcpy/Size.java \
	com/genymobile/scrcpy/wrappers/DisplayManager.java \
	com/genymobile/scrcpy/wrappers/InputManager.java \
	com/genymobile/scrcpy/wrappers/ServiceManager.java \
	com/genymobile/scrcpy/wrappers/WindowManager.java \

TEST_SRC := com/genymobile/scrcpy/ControlEventReaderTest.java \

# generate classnames from filepath
TEST_CLS := $(subst /,.,$(basename $(TEST_SRC)))

JAR := scrcpy-server.jar
MAIN := com.genymobile.scrcpy.ScrCpyServer

AIDL_GEN := $(AIDL_SRC:%.aidl=$(GEN_DIR)/%.java)
AIDL_CLS := $(AIDL_SRC:%.aidl=$(CLS_DIR)/%.class)
SRC_CLS := $(SRC:%.java=$(CLS_DIR)/%.class)
CLS := $(AIDL_CLS) $(SRC_CLS)

ALL_JAVA := $(AIDL_GEN) $(addprefix $(SRC_DIR)/,$(SRC))
ALL_TESTS := $(addprefix $(TEST_SRC_DIR)/,$(TEST_SRC))

jar: $(JAR)

$(AIDL_GEN): $(GEN_DIR)/%.java : $(SRC_DIR)/%.aidl
	mkdir -p $(GEN_DIR)
	"$(AIDL)" -o$(GEN_DIR) $(SRC_DIR)/$(AIDL_SRC)

compile: $(ALL_JAVA)
	@mkdir -p "$(CLS_DIR)"
	javac -source 1.7 -target 1.7 \
		-cp "$(ANDROID_JAR)" \
		-d "$(CLS_DIR)" -sourcepath $(SRC_DIR):$(GEN_DIR) \
		$(ALL_JAVA)

$(JAR): $(ALL_JAVA)
	# we cannot track easily class dependencies, so execute compile only when jar is outdated
	+$(MAKE) compile
	"$(DX)" --dex --output=$(CLS_DEX) $(CLS_DIR)
	jar cvf $(JAR) classes.dex

push: jar
	adb push $(JAR) /data/local/tmp/

run: push
	adb shell "CLASSPATH=/data/local/tmp/$(JAR) app_process /system/bin $(MAIN)"

clean:
	rm -rf $(CLS_DEX) $(CLS_DIR) $(GEN_DIR) $(JAR) $(TEST_CLS_DIR)

compiletests: compile $(ALL_TESTS)
	@mkdir -p "$(TEST_CLS_DIR)"
	javac -cp "$(TEST_LIBS):$(ANDROID_JAR):$(CLS_DIR)" -d "$(TEST_CLS_DIR)" -sourcepath "$(TEST_SRC_DIR)" $(ALL_TESTS)

test:
	+$(MAKE) compiletests
	java -cp "$(TEST_LIBS):$(ANDROID_JAR):$(CLS_DIR):$(TEST_CLS_DIR)" org.junit.runner.JUnitCore $(TEST_CLS)
