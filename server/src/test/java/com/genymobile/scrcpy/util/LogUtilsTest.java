package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.display.DisplayInfo;
import com.genymobile.scrcpy.model.Size;

import android.view.Display;
import org.junit.Assert;
import org.junit.Test;

import java.util.Arrays;
import java.util.Collections;

public class LogUtilsTest {

    @Test
    public void testDisplayFlagNames() {
        Assert.assertEquals(Collections.emptyList(), LogUtils.getDisplayFlagNames(0));
        Assert.assertEquals(Collections.singletonList("FLAG_SECURE"), LogUtils.getDisplayFlagNames(Display.FLAG_SECURE));

        // Flags are listed from the lowest bit to the highest
        int flags = Display.FLAG_PRESENTATION | Display.FLAG_SUPPORTS_PROTECTED_BUFFERS | Display.FLAG_PRIVATE;
        Assert.assertEquals(Arrays.asList("FLAG_SUPPORTS_PROTECTED_BUFFERS", "FLAG_PRIVATE", "FLAG_PRESENTATION"),
                LogUtils.getDisplayFlagNames(flags));
    }

    @Test
    public void testDisplayFlagNamesUnknown() {
        // A flag which has no name on this platform is reported by its value
        int unknown = 1 << 27;
        Assert.assertEquals(Collections.singletonList("0x8000000"), LogUtils.getDisplayFlagNames(unknown));
        Assert.assertEquals(Arrays.asList("FLAG_SECURE", "0x8000000"), LogUtils.getDisplayFlagNames(Display.FLAG_SECURE | unknown));
        Assert.assertEquals(Collections.singletonList("0x80000000"), LogUtils.getDisplayFlagNames(1 << 31));
    }

    @Test
    public void testDisplayDescription() {
        int flags = Display.FLAG_SECURE | Display.FLAG_SUPPORTS_PROTECTED_BUFFERS;
        DisplayInfo displayInfo = new DisplayInfo(0, new Size(1080, 2340), 0, 0, flags, 420, "local:0", DisplayInfo.TYPE_INTERNAL,
                "Built-in Screen");
        Assert.assertEquals("1080x2340, 420dpi, internal, \"Built-in Screen\", flags=[FLAG_SUPPORTS_PROTECTED_BUFFERS, FLAG_SECURE]",
                LogUtils.buildDisplayDescription(displayInfo));
    }

    @Test
    public void testDisplayDescriptionWithoutDetails() {
        // The type and the name are unknown when the display info is parsed from "dumpsys display"
        DisplayInfo displayInfo = new DisplayInfo(2, new Size(1920, 1080), 0, 2, 0, 160, null, DisplayInfo.TYPE_UNKNOWN, null);
        Assert.assertEquals("1920x1080, 160dpi", LogUtils.buildDisplayDescription(displayInfo));
    }

    @Test
    public void testDisplayDescriptionTypes() {
        Assert.assertEquals("1920x1080, 160dpi, external, \"HDMI Screen\"", describe(DisplayInfo.TYPE_EXTERNAL, "HDMI Screen"));
        Assert.assertEquals("1920x1080, 160dpi, wifi, \"TV\"", describe(DisplayInfo.TYPE_WIFI, "TV"));
        Assert.assertEquals("1920x1080, 160dpi, overlay, \"Overlay #1\"", describe(DisplayInfo.TYPE_OVERLAY, "Overlay #1"));
        Assert.assertEquals("1920x1080, 160dpi, virtual, \"scrcpy\"", describe(DisplayInfo.TYPE_VIRTUAL, "scrcpy"));
        // An unknown type is not printed
        Assert.assertEquals("1920x1080, 160dpi, \"Other\"", describe(42, "Other"));
    }

    private static String describe(int type, String name) {
        DisplayInfo displayInfo = new DisplayInfo(2, new Size(1920, 1080), 0, 2, 0, 160, null, type, name);
        return LogUtils.buildDisplayDescription(displayInfo);
    }
}
