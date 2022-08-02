package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Command;
import com.genymobile.scrcpy.DisplayInfo;
import com.genymobile.scrcpy.Ln;
import com.genymobile.scrcpy.Size;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class DisplayManager {
    private final Object manager; // instance of hidden class android.hardware.display.DisplayManagerGlobal

    public DisplayManager(Object manager) {
        this.manager = manager;
    }

    private DisplayInfo parseDumpsysDisplay(int displayId) {
        try {
            String dumpDisplay = Command.execReadOutput("dumpsys", "display");
            Pattern regex = Pattern.compile(
                    "^    mBaseDisplayInfo=DisplayInfo\\{\".*\", displayId " + displayId + ".+, real ([0-9]+) x ([0-9]+).+, rotation ([0-9]+).*, " +
                            "layerStack ([0-9]+).*");
            Matcher m = regex.matcher(dumpDisplay);
            if (!m.find()) {
                return null;
            }
            int width = Integer.parseInt(m.group(1));
            int height = Integer.parseInt(m.group(2));
            int rotation = Integer.parseInt(m.group(3));
            int layerStack = Integer.parseInt(m.group(4));
            int flags = 0; // TODO
            return new DisplayInfo(displayId, new Size(width, height), rotation, layerStack, flags);
        } catch (Exception e) {
            Ln.e("Could not get display info from \"dumpsys display\" output", e);
            return null;
        }
    }

    public DisplayInfo getDisplayInfo(int displayId) {
        try {
            Object displayInfo = manager.getClass().getMethod("getDisplayInfo", int.class).invoke(manager, displayId);
            if (displayInfo == null) {
                // fallback when displayInfo is null
                return parseDumpsysDisplay(displayId);
            }
            Class<?> cls = displayInfo.getClass();
            // width and height already take the rotation into account
            int width = cls.getDeclaredField("logicalWidth").getInt(displayInfo);
            int height = cls.getDeclaredField("logicalHeight").getInt(displayInfo);
            int rotation = cls.getDeclaredField("rotation").getInt(displayInfo);
            int layerStack = cls.getDeclaredField("layerStack").getInt(displayInfo);
            int flags = cls.getDeclaredField("flags").getInt(displayInfo);
            return new DisplayInfo(displayId, new Size(width, height), rotation, layerStack, flags);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public int[] getDisplayIds() {
        try {
            return (int[]) manager.getClass().getMethod("getDisplayIds").invoke(manager);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }
}
