package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.DisplayInfo;
import com.genymobile.scrcpy.Ln;
import com.genymobile.scrcpy.Size;

import android.os.IInterface;
import android.hardware.display.VirtualDisplay;
import android.view.Display;
import android.view.Surface;

import java.lang.reflect.Method;

public final class DisplayManager {
    private final IInterface manager;
    private Method createVirtualDisplayMethod;

    public DisplayManager(IInterface manager) {
        this.manager = manager;
    }

    public DisplayInfo getDisplayInfo(int displayId) {
        try {
            Object displayInfo = manager.getClass().getMethod("getDisplayInfo", int.class).invoke(manager, displayId);
            if (displayInfo == null) {
                return null;
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
        Method method;
        String methodName = "getDisplayIds";
        try {
            method = manager.getClass().getMethod(methodName);
            return (int[]) method.invoke(manager);
        } catch (NoSuchMethodException e) {
            Ln.e("FIXME: Returning only default display.");
            Ln.e("See https://github.com/NetrisTV/ws-scrcpy/issues/217");
            return new int[]{Display.DEFAULT_DISPLAY};
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    private Method getCreateVirtualDisplayMethod() throws NoSuchMethodException {
        if (createVirtualDisplayMethod == null) {
            createVirtualDisplayMethod = android.hardware.display.DisplayManager.class
                    .getMethod("createVirtualDisplay", String.class, int.class, int.class, int.class, Surface.class);
        }
        return createVirtualDisplayMethod;
    }

    public VirtualDisplay createVirtualDisplay(String name, int width, int height, int displayIdToMirror, Surface surface) throws Exception {
        Method method = getCreateVirtualDisplayMethod();
        return (VirtualDisplay) method.invoke(null, name, width, height, displayIdToMirror, surface);
    }
}
