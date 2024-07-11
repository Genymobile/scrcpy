package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.wrappers.DisplayManager;

import android.view.Display;
import org.junit.Assert;
import org.junit.Test;

public class CommandParserTest {
    @Test
    public void testParseDisplayInfoFromDumpsysDisplay() {
        /* @formatter:off */
        String partialOutput = "Logical Displays: size=1\n"
                + "  Display 0:\n"
                + "mDisplayId=0\n"
                + "    mLayerStack=0\n"
                + "    mHasContent=true\n"
                + "    mDesiredDisplayModeSpecs={baseModeId=2 primaryRefreshRateRange=[90 90] appRequestRefreshRateRange=[90 90]}\n"
                + "    mRequestedColorMode=0\n"
                + "    mDisplayOffset=(0, 0)\n"
                + "    mDisplayScalingDisabled=false\n"
                + "    mPrimaryDisplayDevice=Built-in Screen\n"
                + "    mBaseDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0, FLAG_SECURE, FLAG_SUPPORTS_PROTECTED_BUFFERS, FLAG_TRUSTED, "
                + "real 1440 x 3120, largest app 1440 x 3120, smallest app 1440 x 3120, appVsyncOff 2000000, presDeadline 11111111, mode 2, "
                + "defaultMode 1, modes [{id=1, width=1440, height=3120, fps=60.0}, {id=2, width=1440, height=3120, fps=90.0}, {id=3, width=1080, "
                + "height=2340, fps=90.0}, {id=4, width=1080, height=2340, fps=60.0}], hdrCapabilities HdrCapabilities{mSupportedHdrTypes=[2, 3, 4], "
                + "mMaxLuminance=540.0, mMaxAverageLuminance=270.1, mMinLuminance=0.2}, minimalPostProcessingSupported false, rotation 0, state OFF, "
                + "type INTERNAL, uniqueId \"local:0\", app 1440 x 3120, density 600 (515.154 x 514.597) dpi, layerStack 0, colorMode 0, "
                + "supportedColorModes [0, 7, 9], address {port=129, model=0}, deviceProductInfo DeviceProductInfo{name=, manufacturerPnpId=QCM, "
                + "productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, relativeAddress=null}, removeMode 0}\n"
                + "    mOverrideDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0, FLAG_SECURE, FLAG_SUPPORTS_PROTECTED_BUFFERS, "
                + "FLAG_TRUSTED, real 1440 x 3120, largest app 3120 x 2983, smallest app 1440 x 1303, appVsyncOff 2000000, presDeadline 11111111, "
                + "mode 2, defaultMode 1, modes [{id=1, width=1440, height=3120, fps=60.0}, {id=2, width=1440, height=3120, fps=90.0}, {id=3, "
                + "width=1080, height=2340, fps=90.0}, {id=4, width=1080, height=2340, fps=60.0}], hdrCapabilities "
                + "HdrCapabilities{mSupportedHdrTypes=[2, 3, 4], mMaxLuminance=540.0, mMaxAverageLuminance=270.1, mMinLuminance=0.2}, "
                + "minimalPostProcessingSupported false, rotation 0, state ON, type INTERNAL, uniqueId \"local:0\", app 1440 x 3120, density 600 "
                + "(515.154 x 514.597) dpi, layerStack 0, colorMode 0, supportedColorModes [0, 7, 9], address {port=129, model=0}, deviceProductInfo "
                + "DeviceProductInfo{name=, manufacturerPnpId=QCM, productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, "
                + "relativeAddress=null}, removeMode 0}\n"
                + "    mRequestedMinimalPostProcessing=false\n";
        DisplayInfo displayInfo = DisplayManager.parseDisplayInfo(partialOutput, 0);
        Assert.assertNotNull(displayInfo);
        Assert.assertEquals(0, displayInfo.getDisplayId());
        Assert.assertEquals(0, displayInfo.getRotation());
        Assert.assertEquals(0, displayInfo.getLayerStack());
        // FLAG_TRUSTED does not exist in Display (@TestApi), so it won't be reported
        Assert.assertEquals(Display.FLAG_SECURE | Display.FLAG_SUPPORTS_PROTECTED_BUFFERS, displayInfo.getFlags());
        Assert.assertEquals(1440, displayInfo.getSize().getWidth());
        Assert.assertEquals(3120, displayInfo.getSize().getHeight());
    }

    @Test
    public void testParseDisplayInfoFromDumpsysDisplayWithRotation() {
        /* @formatter:off */
        String partialOutput = "Logical Displays: size=1\n"
                + "  Display 0:\n"
                + "mDisplayId=0\n"
                + "    mLayerStack=0\n"
                + "    mHasContent=true\n"
                + "    mDesiredDisplayModeSpecs={baseModeId=2 primaryRefreshRateRange=[90 90] appRequestRefreshRateRange=[90 90]}\n"
                + "    mRequestedColorMode=0\n"
                + "    mDisplayOffset=(0, 0)\n"
                + "    mDisplayScalingDisabled=false\n"
                + "    mPrimaryDisplayDevice=Built-in Screen\n"
                + "    mBaseDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0, FLAG_SECURE, FLAG_SUPPORTS_PROTECTED_BUFFERS, FLAG_TRUSTED, "
                + "real 1440 x 3120, largest app 1440 x 3120, smallest app 1440 x 3120, appVsyncOff 2000000, presDeadline 11111111, mode 2, "
                + "defaultMode 1, modes [{id=1, width=1440, height=3120, fps=60.0}, {id=2, width=1440, height=3120, fps=90.0}, {id=3, width=1080, "
                + "height=2340, fps=90.0}, {id=4, width=1080, height=2340, fps=60.0}], hdrCapabilities HdrCapabilities{mSupportedHdrTypes=[2, 3, 4], "
                + "mMaxLuminance=540.0, mMaxAverageLuminance=270.1, mMinLuminance=0.2}, minimalPostProcessingSupported false, rotation 0, state ON, "
                + "type INTERNAL, uniqueId \"local:0\", app 1440 x 3120, density 600 (515.154 x 514.597) dpi, layerStack 0, colorMode 0, "
                + "supportedColorModes [0, 7, 9], address {port=129, model=0}, deviceProductInfo DeviceProductInfo{name=, manufacturerPnpId=QCM, "
                + "productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, relativeAddress=null}, removeMode 0}\n"
                + "    mOverrideDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0, FLAG_SECURE, FLAG_SUPPORTS_PROTECTED_BUFFERS, "
                + "FLAG_TRUSTED, real 3120 x 1440, largest app 3120 x 2983, smallest app 1440 x 1303, appVsyncOff 2000000, presDeadline 11111111, "
                + "mode 2, defaultMode 1, modes [{id=1, width=1440, height=3120, fps=60.0}, {id=2, width=1440, height=3120, fps=90.0}, {id=3, "
                + "width=1080, height=2340, fps=90.0}, {id=4, width=1080, height=2340, fps=60.0}], hdrCapabilities "
                + "HdrCapabilities{mSupportedHdrTypes=[2, 3, 4], mMaxLuminance=540.0, mMaxAverageLuminance=270.1, mMinLuminance=0.2}, "
                + "minimalPostProcessingSupported false, rotation 3, state ON, type INTERNAL, uniqueId \"local:0\", app 3120 x 1440, density 600 "
                + "(515.154 x 514.597) dpi, layerStack 0, colorMode 0, supportedColorModes [0, 7, 9], address {port=129, model=0}, deviceProductInfo "
                + "DeviceProductInfo{name=, manufacturerPnpId=QCM, productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, "
                + "relativeAddress=null}, removeMode 0}\n"
                + "    mRequestedMinimalPostProcessing=false";
        DisplayInfo displayInfo = DisplayManager.parseDisplayInfo(partialOutput, 0);
        Assert.assertNotNull(displayInfo);
        Assert.assertEquals(0, displayInfo.getDisplayId());
        Assert.assertEquals(3, displayInfo.getRotation());
        Assert.assertEquals(0, displayInfo.getLayerStack());
        // FLAG_TRUSTED does not exist in Display (@TestApi), so it won't be reported
        Assert.assertEquals(Display.FLAG_SECURE | Display.FLAG_SUPPORTS_PROTECTED_BUFFERS, displayInfo.getFlags());
        Assert.assertEquals(3120, displayInfo.getSize().getWidth());
        Assert.assertEquals(1440, displayInfo.getSize().getHeight());
    }

    @Test
    public void testParseDisplayInfoFromDumpsysDisplayAPI31() {
        /* @formatter:off */
        String partialOutput = "Logical Displays: size=1\n"
                + "  Display 0:\n"
                + "    mDisplayId=0\n"
                + "    mPhase=1\n"
                + "    mLayerStack=0\n"
                + "    mHasContent=true\n"
                + "    mDesiredDisplayModeSpecs={baseModeId=1 allowGroupSwitching=false primaryRefreshRateRange=[0 60] appRequestRefreshRateRange=[0 "
                + "Infinity]}\n"
                + "    mRequestedColorMode=0\n"
                + "    mDisplayOffset=(0, 0)\n"
                + "    mDisplayScalingDisabled=false\n"
                + "    mPrimaryDisplayDevice=Built-in Screen\n"
                + "    mBaseDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0\", displayGroupId 0, FLAG_SECURE, "
                + "FLAG_SUPPORTS_PROTECTED_BUFFERS, FLAG_TRUSTED, real 1080 x 2280, largest app 1080 x 2280, smallest app 1080 x 2280, appVsyncOff "
                + "1000000, presDeadline 16666666, mode 1, defaultMode 1, modes [{id=1, width=1080, height=2280, fps=60.000004, "
                + "alternativeRefreshRates=[]}], hdrCapabilities HdrCapabilities{mSupportedHdrTypes=[], mMaxLuminance=500.0, "
                + "mMaxAverageLuminance=500.0, mMinLuminance=0.0}, userDisabledHdrTypes [], minimalPostProcessingSupported false, rotation 0, state "
                + "ON, type INTERNAL, uniqueId \"local:0\", app 1080 x 2280, density 440 (440.0 x 440.0) dpi, layerStack 0, colorMode 0, "
                + "supportedColorModes [0], address {port=0, model=0}, deviceProductInfo DeviceProductInfo{name=EMU_display_0, "
                + "manufacturerPnpId=GGL, productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, connectionToSinkType=0}, "
                + "removeMode 0, refreshRateOverride 0.0, brightnessMinimum 0.0, brightnessMaximum 1.0, brightnessDefault 0.39763778}\n"
                + "    mOverrideDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0\", displayGroupId 0, FLAG_SECURE, "
                + "FLAG_SUPPORTS_PROTECTED_BUFFERS, FLAG_TRUSTED, real 1080 x 2280, largest app 2148 x 2065, smallest app 1080 x 997, appVsyncOff "
                + "1000000, presDeadline 16666666, mode 1, defaultMode 1, modes [{id=1, width=1080, height=2280, fps=60.000004, "
                + "alternativeRefreshRates=[]}], hdrCapabilities HdrCapabilities{mSupportedHdrTypes=[], mMaxLuminance=500.0, "
                + "mMaxAverageLuminance=500.0, mMinLuminance=0.0}, userDisabledHdrTypes [], minimalPostProcessingSupported false, rotation 0, state "
                + "ON, type INTERNAL, uniqueId \"local:0\", app 1080 x 2148, density 440 (440.0 x 440.0) dpi, layerStack 0, colorMode 0, "
                + "supportedColorModes [0], address {port=0, model=0}, deviceProductInfo DeviceProductInfo{name=EMU_display_0, "
                + "manufacturerPnpId=GGL, productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, connectionToSinkType=0}, "
                + "removeMode 0, refreshRateOverride 0.0, brightnessMinimum 0.0, brightnessMaximum 1.0, brightnessDefault 0.39763778}\n"
                + "    mRequestedMinimalPostProcessing=false\n"
                + "    mFrameRateOverrides=[]\n"
                + "    mPendingFrameRateOverrideUids={}\n";
        DisplayInfo displayInfo = DisplayManager.parseDisplayInfo(partialOutput, 0);
        Assert.assertNotNull(displayInfo);
        Assert.assertEquals(0, displayInfo.getDisplayId());
        Assert.assertEquals(0, displayInfo.getRotation());
        Assert.assertEquals(0, displayInfo.getLayerStack());
        // FLAG_TRUSTED does not exist in Display (@TestApi), so it won't be reported
        Assert.assertEquals(Display.FLAG_SECURE | Display.FLAG_SUPPORTS_PROTECTED_BUFFERS, displayInfo.getFlags());
        Assert.assertEquals(1080, displayInfo.getSize().getWidth());
        Assert.assertEquals(2280, displayInfo.getSize().getHeight());
    }

    @Test
    public void testParseDisplayInfoFromDumpsysDisplayAPI31NoFlags() {
        /* @formatter:off */
        String partialOutput = "Logical Displays: size=1\n"
                + "  Display 0:\n"
                + "    mDisplayId=0\n"
                + "    mPhase=1\n"
                + "    mLayerStack=0\n"
                + "    mHasContent=true\n"
                + "    mDesiredDisplayModeSpecs={baseModeId=1 allowGroupSwitching=false primaryRefreshRateRange=[0 60] appRequestRefreshRateRange=[0 "
                + "Infinity]}\n"
                + "    mRequestedColorMode=0\n"
                + "    mDisplayOffset=(0, 0)\n"
                + "    mDisplayScalingDisabled=false\n"
                + "    mPrimaryDisplayDevice=Built-in Screen\n"
                + "    mBaseDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0\", displayGroupId 0, "
                + "real 1080 x 2280, largest app 1080 x 2280, smallest app 1080 x 2280, appVsyncOff "
                + "1000000, presDeadline 16666666, mode 1, defaultMode 1, modes [{id=1, width=1080, height=2280, fps=60.000004, "
                + "alternativeRefreshRates=[]}], hdrCapabilities HdrCapabilities{mSupportedHdrTypes=[], mMaxLuminance=500.0, "
                + "mMaxAverageLuminance=500.0, mMinLuminance=0.0}, userDisabledHdrTypes [], minimalPostProcessingSupported false, rotation 0, state "
                + "ON, type INTERNAL, uniqueId \"local:0\", app 1080 x 2280, density 440 (440.0 x 440.0) dpi, layerStack 0, colorMode 0, "
                + "supportedColorModes [0], address {port=0, model=0}, deviceProductInfo DeviceProductInfo{name=EMU_display_0, "
                + "manufacturerPnpId=GGL, productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, connectionToSinkType=0}, "
                + "removeMode 0, refreshRateOverride 0.0, brightnessMinimum 0.0, brightnessMaximum 1.0, brightnessDefault 0.39763778}\n"
                + "    mOverrideDisplayInfo=DisplayInfo{\"Built-in Screen\", displayId 0\", displayGroupId 0, "
                + "real 1080 x 2280, largest app 2148 x 2065, smallest app 1080 x 997, appVsyncOff "
                + "1000000, presDeadline 16666666, mode 1, defaultMode 1, modes [{id=1, width=1080, height=2280, fps=60.000004, "
                + "alternativeRefreshRates=[]}], hdrCapabilities HdrCapabilities{mSupportedHdrTypes=[], mMaxLuminance=500.0, "
                + "mMaxAverageLuminance=500.0, mMinLuminance=0.0}, userDisabledHdrTypes [], minimalPostProcessingSupported false, rotation 0, state "
                + "ON, type INTERNAL, uniqueId \"local:0\", app 1080 x 2148, density 440 (440.0 x 440.0) dpi, layerStack 0, colorMode 0, "
                + "supportedColorModes [0], address {port=0, model=0}, deviceProductInfo DeviceProductInfo{name=EMU_display_0, "
                + "manufacturerPnpId=GGL, productId=1, modelYear=null, manufactureDate=ManufactureDate{week=27, year=2006}, connectionToSinkType=0}, "
                + "removeMode 0, refreshRateOverride 0.0, brightnessMinimum 0.0, brightnessMaximum 1.0, brightnessDefault 0.39763778}\n"
                + "    mRequestedMinimalPostProcessing=false\n"
                + "    mFrameRateOverrides=[]\n"
                + "    mPendingFrameRateOverrideUids={}\n";
        DisplayInfo displayInfo = DisplayManager.parseDisplayInfo(partialOutput, 0);
        Assert.assertNotNull(displayInfo);
        Assert.assertEquals(0, displayInfo.getDisplayId());
        Assert.assertEquals(0, displayInfo.getRotation());
        Assert.assertEquals(0, displayInfo.getLayerStack());
        Assert.assertEquals(0, displayInfo.getFlags());
        Assert.assertEquals(1080, displayInfo.getSize().getWidth());
        Assert.assertEquals(2280, displayInfo.getSize().getHeight());
    }

    @Test
    public void testParseDisplayInfoFromDumpsysDisplayAPI29WithNoFlags() {
        /* @formatter:off */
        String partialOutput = "Logical Displays: size=2\n"
                + "  Display 0:\n"
                + "    mDisplayId=0\n"
                + "    mLayerStack=0\n"
                + "    mHasContent=true\n"
                + "    mAllowedDisplayModes=[1]\n"
                + "    mRequestedColorMode=0\n"
                + "    mDisplayOffset=(0, 0)\n"
                + "    mDisplayScalingDisabled=false\n"
                + "    mPrimaryDisplayDevice=Built-in Screen\n"
                + "    mBaseDisplayInfo=DisplayInfo{\"Built-in Screen, displayId 0\", uniqueId \"local:0\", app 3664 x 1920, "
                + "real 3664 x 1920, largest app 3664 x 1920, smallest app 3664 x 1920, mode 61, defaultMode 61, modes ["
                + "{id=1, width=3664, height=1920, fps=60.000004}, {id=2, width=3664, height=1920, fps=61.000004}, "
                + "{id=61, width=3664, height=1920, fps=120.00001}], colorMode 0, supportedColorModes [0], "
                + "hdrCapabilities android.view.Display$HdrCapabilities@4a41fe79, rotation 0, density 290 (320.842 x 319.813) dpi, "
                + "layerStack 0, appVsyncOff 1000000, presDeadline 8333333, type BUILT_IN, address {port=129, model=0}, "
                + "state ON, FLAG_SECURE, FLAG_SUPPORTS_PROTECTED_BUFFERS, removeMode 0}\n"
                + "    mOverrideDisplayInfo=DisplayInfo{\"Built-in Screen, displayId 0\", uniqueId \"local:0\", app 3664 x 1920, "
                + "real 3664 x 1920, largest app 3664 x 3620, smallest app 1920 x 1876, mode 61, defaultMode 61, modes ["
                + "{id=1, width=3664, height=1920, fps=60.000004}, {id=2, width=3664, height=1920, fps=61.000004}, "
                + "{id=61, width=3664, height=1920, fps=120.00001}], colorMode 0, supportedColorModes [0], "
                + "hdrCapabilities android.view.Display$HdrCapabilities@4a41fe79, rotation 0, density 290 (320.842 x 319.813) dpi, "
                + "layerStack 0, appVsyncOff 1000000, presDeadline 8333333, type BUILT_IN, address {port=129, model=0}, "
                + "state ON, FLAG_SECURE, FLAG_SUPPORTS_PROTECTED_BUFFERS, removeMode 0}\n"
                + "  Display 31:\n"
                + "    mDisplayId=31\n"
                + "    mLayerStack=31\n"
                + "    mHasContent=true\n"
                + "    mAllowedDisplayModes=[92]\n"
                + "    mRequestedColorMode=0\n"
                + "    mDisplayOffset=(0, 0)\n"
                + "    mDisplayScalingDisabled=false\n"
                + "    mPrimaryDisplayDevice=PanelLayer-#main\n"
                + "    mBaseDisplayInfo=DisplayInfo{\"PanelLayer-#main, displayId 31\", uniqueId "
                + "\"virtual:com.test.system,10040,PanelLayer-#main,0\", app 800 x 110, real 800 x 110, largest app 800 x 110, smallest app 800 x "
                + "110, mode 92, defaultMode 92, modes [{id=92, width=800, height=110, fps=60.0}], colorMode 0, supportedColorModes [0], "
                + "hdrCapabilities null, rotation 0, density 200 (200.0 x 200.0) dpi, layerStack 31, appVsyncOff 0, presDeadline 16666666, "
                + "type VIRTUAL, state ON, owner com.test.system (uid 10040), FLAG_PRIVATE, removeMode 1}\n"
                + "    mOverrideDisplayInfo=DisplayInfo{\"PanelLayer-#main, displayId 31\", uniqueId "
                + "\"virtual:com.test.system,10040,PanelLayer-#main,0\", app 800 x 110, real 800 x 110, largest app 800 x 800, smallest app 110 x "
                + "110, mode 92, defaultMode 92, modes [{id=92, width=800, height=110, fps=60.0}], colorMode 0, supportedColorModes [0], "
                + "hdrCapabilities null, rotation 0, density 200 (200.0 x 200.0) dpi, layerStack 31, appVsyncOff 0, presDeadline 16666666, "
                + "type VIRTUAL, state OFF, owner com.test.system (uid 10040), FLAG_PRIVATE, removeMode 1}\n";
        DisplayInfo displayInfo = DisplayManager.parseDisplayInfo(partialOutput, 31);
        Assert.assertNotNull(displayInfo);
        Assert.assertEquals(31, displayInfo.getDisplayId());
        Assert.assertEquals(0, displayInfo.getRotation());
        Assert.assertEquals(31, displayInfo.getLayerStack());
        Assert.assertEquals(0, displayInfo.getFlags());
        Assert.assertEquals(800, displayInfo.getSize().getWidth());
        Assert.assertEquals(110, displayInfo.getSize().getHeight());
    }
}
