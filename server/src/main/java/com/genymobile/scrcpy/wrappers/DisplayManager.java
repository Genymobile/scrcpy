package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.DisplayInfo;
import com.genymobile.scrcpy.Ln;
import com.genymobile.scrcpy.Size;

import android.os.Build;
import android.os.IInterface;
import android.view.Display;

import java.lang.reflect.Method;

public final class DisplayManager {
    private final IInterface manager;

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
            Ln.d("Failed to get display ids");
            Ln.d("Available methods:");
            for (Method m: manager.getClass().getMethods()) {
                Ln.d(m.getName() + " parameters:");
                if (m.getName().equals(methodName)) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        if (m.getParameterCount() > 0) {
                            for (Class c : m.getParameterTypes()) {
                                Ln.d(c.getName());
                            }
                        } else {
                            Ln.d("No parameters");
                        }
                    }
                }
                Ln.d("Return type: " + m.getReturnType().getName());
            }
            Ln.d("///");
            return new int[]{Display.DEFAULT_DISPLAY};
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }
}
