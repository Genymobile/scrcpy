.PHONY: jar push run clean

SRC_DIR := src
GEN_DIR := gen
CLS_DIR := classes
CLS_DEX := classes.dex

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
	com/genymobile/scrcpy/DesktopConnection.java \
	com/genymobile/scrcpy/ScreenInfo.java \
	com/genymobile/scrcpy/ScreenStreamer.java \
	com/genymobile/scrcpy/ScreenStreamerSession.java \
	com/genymobile/scrcpy/ScreenUtil.java \

JAR := scrcpy-server.jar
MAIN := com.genymobile.scrcpy.ScrCpyServer

AIDL_GEN := $(AIDL_SRC:%.aidl=$(GEN_DIR)/%.java)
AIDL_CLS := $(AIDL_SRC:%.aidl=$(CLS_DIR)/%.class)
SRC_CLS := $(SRC:%.java=$(CLS_DIR)/%.class)
CLS := $(AIDL_CLS) $(SRC_CLS)

ALL_JAVA := $(AIDL_GEN) $(addprefix $(SRC_DIR)/,$(SRC))

jar: $(JAR)

$(AIDL_GEN): $(GEN_DIR)/%.java : $(SRC_DIR)/%.aidl
	mkdir -p $(GEN_DIR)
	"$(AIDL)" -o$(GEN_DIR) $(SRC_DIR)/$(AIDL_SRC)


$(JAR): $(ALL_JAVA)
	@mkdir -p $(CLS_DIR)
	javac -source 1.7 -target 1.7 \
		-cp "$(ANDROID_JAR)" \
		-d "$(CLS_DIR)" -sourcepath $(SRC_DIR):$(GEN_DIR) \
		$(ALL_JAVA)
	"$(DX)" --dex --output=$(CLS_DEX) $(CLS_DIR)
	jar cvf $(JAR) classes.dex

push: jar
	adb push $(JAR) /data/local/tmp/

run: push
	adb shell "CLASSPATH=/data/local/tmp/$(JAR) app_process /system/bin $(MAIN)"

clean:
	rm -rf $(CLS_DEX) $(CLS_DIR) $(GEN_DIR) $(JAR)
