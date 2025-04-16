package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Command;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.hardware.display.VirtualDisplay;
import android.os.Handler;
import android.view.Display;
import android.view.Surface;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class DisplayManager {

    // android.hardware.display.DisplayManager.EVENT_FLAG_DISPLAY_CHANGED
    public static final long EVENT_FLAG_DISPLAY_CHANGED = 1L << 2;

    public interface DisplayListener {
        /**
         * Called whenever the properties of a logical {@link android.view.Display},
         * such as size and density, have changed.
         *
         * @param displayId The id of the logical display that changed.
         */
        void onDisplayChanged(int displayId);
    }

    public static final class DisplayListenerHandle {
        private final Object displayListenerProxy;
        private DisplayListenerHandle(Object displayListenerProxy) {
            this.displayListenerProxy = displayListenerProxy;
        }
    }

    private final Object manager; // instance of hidden class android.hardware.display.DisplayManagerGlobal
    private Method getDisplayInfoMethod;
    private Method createVirtualDisplayMethod;
    private Method requestDisplayPowerMethod;

    static DisplayManager create() {
        try {
            Class<?> clazz = Class.forName("android.hardware.display.DisplayManagerGlobal");
            Method getInstanceMethod = clazz.getDeclaredMethod("getInstance");
            Object dmg = getInstanceMethod.invoke(null);
            return new DisplayManager(dmg);
        } catch (ReflectiveOperationException e) {
            throw new AssertionError(e);
        }
    }

    private DisplayManager(Object manager) {
        this.manager = manager;
    }

    // public to call it from unit tests
    public static DisplayInfo parseDisplayInfo(String dumpsysDisplayOutput, int displayId) {
        Pattern regex = Pattern.compile(
                "^    mOverrideDisplayInfo=DisplayInfo\\{\".*?, displayId " + displayId + ".*?(, FLAG_.*)?, real ([0-9]+) x ([0-9]+).*?, "
                        + "rotation ([0-9]+).*?, density ([0-9]+).*?, layerStack ([0-9]+)",
                Pattern.MULTILINE);
        Matcher m = regex.matcher(dumpsysDisplayOutput);
        if (!m.find()) {
            return null;
        }
        int flags = parseDisplayFlags(m.group(1));
        int width = Integer.parseInt(m.group(2));
        int height = Integer.parseInt(m.group(3));
        int rotation = Integer.parseInt(m.group(4));
        int density = Integer.parseInt(m.group(5));
        int layerStack = Integer.parseInt(m.group(6));

        return new DisplayInfo(displayId, new Size(width, height), rotation, layerStack, flags, density, null);
    }

    private static DisplayInfo getDisplayInfoFromDumpsysDisplay(int displayId) {
        try {
            String dumpsysDisplayOutput = Command.execReadOutput("dumpsys", "display");
            return parseDisplayInfo(dumpsysDisplayOutput, displayId);
        } catch (Exception e) {
            Ln.e("Could not get display info from \"dumpsys display\" output", e);
            return null;
        }
    }

    private static int parseDisplayFlags(String text) {
        if (text == null) {
            return 0;
        }

        int flags = 0;
        Pattern regex = Pattern.compile("FLAG_[A-Z_]+");
        Matcher m = regex.matcher(text);
        while (m.find()) {
            String flagString = m.group();
            try {
                Field filed = Display.class.getDeclaredField(flagString);
                flags |= filed.getInt(null);
            } catch (ReflectiveOperationException e) {
                // Silently ignore, some flags reported by "dumpsys display" are @TestApi
            }
        }
        return flags;
    }

    // getDisplayInfo() may be used from both the Controller thread and the video (main) thread
    private synchronized Method getGetDisplayInfoMethod() throws NoSuchMethodException {
        if (getDisplayInfoMethod == null) {
            getDisplayInfoMethod = manager.getClass().getMethod("getDisplayInfo", int.class);
        }
        return getDisplayInfoMethod;
    }

    public DisplayInfo getDisplayInfo(int displayId) {
        try {
            Method method = getGetDisplayInfoMethod();
            Object displayInfo = method.invoke(manager, displayId);
            if (displayInfo == null) {
                // fallback when displayInfo is null
                return getDisplayInfoFromDumpsysDisplay(displayId);
            }
            Class<?> cls = displayInfo.getClass();
            // width and height already take the rotation into account
            int width = cls.getDeclaredField("logicalWidth").getInt(displayInfo);
            int height = cls.getDeclaredField("logicalHeight").getInt(displayInfo);
            int rotation = cls.getDeclaredField("rotation").getInt(displayInfo);
            int layerStack = cls.getDeclaredField("layerStack").getInt(displayInfo);
            int flags = cls.getDeclaredField("flags").getInt(displayInfo);
            int dpi = cls.getDeclaredField("logicalDensityDpi").getInt(displayInfo);
            String uniqueId = (String) cls.getDeclaredField("uniqueId").get(displayInfo);
            return new DisplayInfo(displayId, new Size(width, height), rotation, layerStack, flags, dpi, uniqueId);
        } catch (ReflectiveOperationException e) {
            throw new AssertionError(e);
        }
    }

    public int[] getDisplayIds() {
        try {
            return (int[]) manager.getClass().getMethod("getDisplayIds").invoke(manager);
        } catch (ReflectiveOperationException e) {
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

    public VirtualDisplay createNewVirtualDisplay(String name, int width, int height, int dpi, Surface surface, int flags) throws Exception {
        Constructor<android.hardware.display.DisplayManager> ctor = android.hardware.display.DisplayManager.class.getDeclaredConstructor(
                Context.class);
        ctor.setAccessible(true);
        android.hardware.display.DisplayManager dm = ctor.newInstance(FakeContext.get());
        return dm.createVirtualDisplay(name, width, height, dpi, surface, flags);
    }

    private Method getRequestDisplayPowerMethod() throws NoSuchMethodException {
        if (requestDisplayPowerMethod == null) {
            requestDisplayPowerMethod = manager.getClass().getMethod("requestDisplayPower", int.class, boolean.class);
        }
        return requestDisplayPowerMethod;
    }

    @TargetApi(AndroidVersions.API_35_ANDROID_15)
    public boolean requestDisplayPower(int displayId, boolean on) {
        try {
            Method method = getRequestDisplayPowerMethod();
            return (boolean) method.invoke(manager, displayId, on);
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }

    public DisplayListenerHandle registerDisplayListener(DisplayListener listener, Handler handler) {
        try {
            Class<?> displayListenerClass = Class.forName("android.hardware.display.DisplayManager$DisplayListener");
            Object displayListenerProxy = Proxy.newProxyInstance(
                    ClassLoader.getSystemClassLoader(),
                    new Class[] {displayListenerClass},
                    (proxy, method, args) -> {
                        if ("onDisplayChanged".equals(method.getName())) {
                            listener.onDisplayChanged((int) args[0]);
                        }
                        if ("toString".equals(method.getName())) {
                            return "DisplayListener";
                        }
                        return null;
                    });
            try {
                manager.getClass()
                        .getMethod("registerDisplayListener", displayListenerClass, Handler.class, long.class, String.class)
                        .invoke(manager, displayListenerProxy, handler, EVENT_FLAG_DISPLAY_CHANGED, FakeContext.PACKAGE_NAME);
            } catch (NoSuchMethodException e) {
                try {
                    manager.getClass()
                            .getMethod("registerDisplayListener", displayListenerClass, Handler.class, long.class)
                            .invoke(manager, displayListenerProxy, handler, EVENT_FLAG_DISPLAY_CHANGED);
                } catch (NoSuchMethodException e2) {
                    manager.getClass()
                            .getMethod("registerDisplayListener", displayListenerClass, Handler.class)
                            .invoke(manager, displayListenerProxy, handler);
                }
            }

            return new DisplayListenerHandle(displayListenerProxy);
        } catch (Exception e) {
            // Rotation and screen size won't be updated, not a fatal error
            Ln.e("Could not register display listener", e);
        }

        return null;
    }

    public void unregisterDisplayListener(DisplayListenerHandle listener) {
        try {
            Class<?> displayListenerClass = Class.forName("android.hardware.display.DisplayManager$DisplayListener");
            manager.getClass().getMethod("unregisterDisplayListener", displayListenerClass).invoke(manager, listener.displayListenerProxy);
        } catch (Exception e) {
            Ln.e("Could not unregister display listener", e);
        }
    }
}
