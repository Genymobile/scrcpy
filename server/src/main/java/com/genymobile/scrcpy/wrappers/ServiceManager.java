package com.genymobile.scrcpy.wrappers;

import android.annotation.SuppressLint;
import android.os.IBinder;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class ServiceManager {

    public static final String PACKAGE_NAME = "com.android.shell";
    public static final int USER_ID = 0;

    private static final Method GET_SERVICE_METHOD;
    static {
        try {
            GET_SERVICE_METHOD = Class.forName("android.os.ServiceManager").getDeclaredMethod("getService", String.class);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    private static WindowManager windowManager;
    private static DisplayManager displayManager;
    private static InputManager inputManager;
    private static PowerManager powerManager;
    private static StatusBarManager statusBarManager;
    private static ClipboardManager clipboardManager;
    private static ActivityManager activityManager;

    private ServiceManager() {
        /* not instantiable */
    }

    private static IInterface getService(String service, String type) {
        try {
            IBinder binder = (IBinder) GET_SERVICE_METHOD.invoke(null, service);
            Method asInterfaceMethod = Class.forName(type + "$Stub").getMethod("asInterface", IBinder.class);
            return (IInterface) asInterfaceMethod.invoke(null, binder);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public static WindowManager getWindowManager() {
        if (windowManager == null) {
            windowManager = new WindowManager(getService("window", "android.view.IWindowManager"));
        }
        return windowManager;
    }

    public static DisplayManager getDisplayManager() {
        if (displayManager == null) {
            try {
                Class<?> clazz = Class.forName("android.hardware.display.DisplayManagerGlobal");
                Method getInstanceMethod = clazz.getDeclaredMethod("getInstance");
                Object dmg = getInstanceMethod.invoke(null);
                displayManager = new DisplayManager(dmg);
            } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
                throw new AssertionError(e);
            }
        }
        return displayManager;
    }

    public static InputManager getInputManager() {
        if (inputManager == null) {
            try {
                Method getInstanceMethod = android.hardware.input.InputManager.class.getDeclaredMethod("getInstance");
                android.hardware.input.InputManager im = (android.hardware.input.InputManager) getInstanceMethod.invoke(null);
                inputManager = new InputManager(im);
            } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
                throw new AssertionError(e);
            }
        }
        return inputManager;
    }

    public static PowerManager getPowerManager() {
        if (powerManager == null) {
            powerManager = new PowerManager(getService("power", "android.os.IPowerManager"));
        }
        return powerManager;
    }

    public static StatusBarManager getStatusBarManager() {
        if (statusBarManager == null) {
            statusBarManager = new StatusBarManager(getService("statusbar", "com.android.internal.statusbar.IStatusBarService"));
        }
        return statusBarManager;
    }

    public static ClipboardManager getClipboardManager() {
        if (clipboardManager == null) {
            IInterface clipboard = getService("clipboard", "android.content.IClipboard");
            if (clipboard == null) {
                // Some devices have no clipboard manager
                // <https://github.com/Genymobile/scrcpy/issues/1440>
                // <https://github.com/Genymobile/scrcpy/issues/1556>
                return null;
            }
            clipboardManager = new ClipboardManager(clipboard);
        }
        return clipboardManager;
    }

    public static ActivityManager getActivityManager() {
        if (activityManager == null) {
            try {
                // On old Android versions, the ActivityManager is not exposed via AIDL,
                // so use ActivityManagerNative.getDefault()
                Class<?> cls = Class.forName("android.app.ActivityManagerNative");
                Method getDefaultMethod = cls.getDeclaredMethod("getDefault");
                IInterface am = (IInterface) getDefaultMethod.invoke(null);
                activityManager = new ActivityManager(am);
            } catch (Exception e) {
                throw new AssertionError(e);
            }
        }

        return activityManager;
    }
}
