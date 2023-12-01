package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Command;
import com.genymobile.scrcpy.DisplayInfo;
import com.genymobile.scrcpy.Ln;
import com.genymobile.scrcpy.Size;

import android.os.Handler;
import android.view.Display;

import java.lang.reflect.Field;
import java.lang.reflect.Proxy;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class DisplayManager {
    /**
     * Event type for when a display is changed.
     *
     * @see #registerDisplayListener(DisplayListener, Handler, long)
     */
    public static final long EVENT_FLAG_DISPLAY_CHANGED = 1L << 2;

    private final Object manager; // instance of hidden class android.hardware.display.DisplayManagerGlobal

    public DisplayManager(Object manager) {
        this.manager = manager;
    }

    // public to call it from unit tests
    public static DisplayInfo parseDisplayInfo(String dumpsysDisplayOutput, int displayId) {
        Pattern regex = Pattern.compile(
                "^    mOverrideDisplayInfo=DisplayInfo\\{\".*?, displayId " + displayId + ".*?(, FLAG_.*)?, real ([0-9]+) x ([0-9]+).*?, "
                        + "rotation ([0-9]+).*?, layerStack ([0-9]+)",
                Pattern.MULTILINE);
        Matcher m = regex.matcher(dumpsysDisplayOutput);
        if (!m.find()) {
            return null;
        }
        int flags = parseDisplayFlags(m.group(1));
        int width = Integer.parseInt(m.group(2));
        int height = Integer.parseInt(m.group(3));
        int rotation = Integer.parseInt(m.group(4));
        int layerStack = Integer.parseInt(m.group(5));

        return new DisplayInfo(displayId, new Size(width, height), rotation, layerStack, flags);
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
        Pattern regex = Pattern.compile("FLAG_[A-Z_]+");
        if (text == null) {
            return 0;
        }

        int flags = 0;
        Matcher m = regex.matcher(text);
        while (m.find()) {
            String flagString = m.group();
            try {
                Field filed = Display.class.getDeclaredField(flagString);
                flags |= filed.getInt(null);
            } catch (NoSuchFieldException | IllegalAccessException e) {
                // Silently ignore, some flags reported by "dumpsys display" are @TestApi
            }
        }
        return flags;
    }

    public DisplayInfo getDisplayInfo(int displayId) {
        try {
            Object displayInfo = manager.getClass().getMethod("getDisplayInfo", int.class).invoke(manager, displayId);
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

    public void registerDisplayListener(DisplayListener listener, Handler handler, long eventMask) {
        try {
            Class<?> displayListenerClass = Class.forName("android.hardware.display.DisplayManager$DisplayListener");
            Object displayListenerProxy = Proxy.newProxyInstance(
                ClassLoader.getSystemClassLoader(),
                new Class[]{ displayListenerClass },
                (proxy, method, args) -> {
                    switch (method.getName()) {
                        case "onDisplayAdded":
                            listener.onDisplayAdded((int) args[0]);
                            break;
                        case "onDisplayRemoved":
                            listener.onDisplayRemoved((int) args[0]);
                            break;
                        case "onDisplayChanged":
                            listener.onDisplayChanged((int) args[0]);
                            break;
                        default:
                            throw new AssertionError("Unexpected method: " + method.getName());
                    }
                    return null;
                });
            try {
                manager
                    .getClass()
                    .getMethod("registerDisplayListener", displayListenerClass, Handler.class, long.class)
                    .invoke(manager, displayListenerProxy, handler, eventMask);
            } catch (NoSuchMethodException e) {
                manager
                    .getClass()
                    .getMethod("registerDisplayListener", displayListenerClass, Handler.class)
                    .invoke(manager, displayListenerProxy, handler);
            }
        } catch (Exception e) {
            // Rotation and screen size won't be updated, not a fatal error
            Ln.w("Could not register display listener", e);
        }
    }

    public interface DisplayListener {
        /**
         * Called whenever a logical display has been added to the system.
         * Use {@link DisplayManager#getDisplay} to get more information about
         * the display.
         *
         * @param displayId The id of the logical display that was added.
         */
        void onDisplayAdded(int displayId);

        /**
         * Called whenever a logical display has been removed from the system.
         *
         * @param displayId The id of the logical display that was removed.
         */
        void onDisplayRemoved(int displayId);

        /**
         * Called whenever the properties of a logical {@link android.view.Display},
         * such as size and density, have changed.
         *
         * @param displayId The id of the logical display that changed.
         */
        void onDisplayChanged(int displayId);
    }
}
