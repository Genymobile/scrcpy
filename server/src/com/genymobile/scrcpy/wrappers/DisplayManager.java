package com.genymobile.scrcpy.wrappers;

import android.os.IInterface;

import com.genymobile.scrcpy.ScreenInfo;

public class DisplayManager {
    private final IInterface manager;

    public DisplayManager(IInterface manager) {
        this.manager = manager;
    }

    public ScreenInfo getScreenInfo() {
        try {
            Object displayInfo = manager.getClass().getMethod("getDisplayInfo", int.class).invoke(manager, 0);
            Class<?> cls = displayInfo.getClass();
            // width and height do not depend on the rotation
            int width = (Integer) cls.getMethod("getNaturalWidth").invoke(displayInfo);
            int height = (Integer) cls.getMethod("getNaturalHeight").invoke(displayInfo);
            int rotation = cls.getDeclaredField("rotation").getInt(displayInfo);
            return new ScreenInfo(width, height, rotation);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }
}
