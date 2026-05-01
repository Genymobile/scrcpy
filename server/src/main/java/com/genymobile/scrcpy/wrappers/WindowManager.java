package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.TargetApi;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.IInterface;
import android.view.IDisplayWindowListener;

import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.List;

public final class WindowManager {

    @SuppressWarnings("checkstyle:LineLength")
    // <https://android.googlesource.com/platform/frameworks/base.git/+/2103ff441c66772c80c8560e322dcd9a45be7dcd/core/java/android/view/WindowManager.java#692>
    public static final int DISPLAY_IME_POLICY_LOCAL = 0;
    public static final int DISPLAY_IME_POLICY_FALLBACK_DISPLAY = 1;
    public static final int DISPLAY_IME_POLICY_HIDE = 2;

    // android.app.WindowConfiguration.WINDOWING_MODE_FREEFORM
    public static final int WINDOWING_MODE_FREEFORM = 5;

    // android.window.DisplayAreaOrganizer.FEATURE_DEFAULT_TASK_CONTAINER
    private static final int FEATURE_DEFAULT_TASK_CONTAINER = 1;

    private final IInterface manager;
    private Method getRotationMethod;

    private Method freezeDisplayRotationMethod;
    private int freezeDisplayRotationMethodVersion;

    private Method isDisplayRotationFrozenMethod;
    private int isDisplayRotationFrozenMethodVersion;

    private Method thawDisplayRotationMethod;
    private int thawDisplayRotationMethodVersion;

    private Method getDisplayImePolicyMethod;
    private Method setDisplayImePolicyMethod;

    static WindowManager create() {
        IInterface manager = ServiceManager.getService("window", "android.view.IWindowManager");
        return new WindowManager(manager);
    }

    private WindowManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getGetRotationMethod() throws NoSuchMethodException {
        if (getRotationMethod == null) {
            Class<?> cls = manager.getClass();
            try {
                // method changed since this commit:
                // https://android.googlesource.com/platform/frameworks/base/+/8ee7285128c3843401d4c4d0412cd66e86ba49e3%5E%21/#F2
                getRotationMethod = cls.getMethod("getDefaultDisplayRotation");
            } catch (NoSuchMethodException e) {
                // old version
                getRotationMethod = cls.getMethod("getRotation");
            }
        }
        return getRotationMethod;
    }

    private Method getFreezeDisplayRotationMethod() throws NoSuchMethodException {
        if (freezeDisplayRotationMethod == null) {
            try {
                // Android 15 preview and 14 QPR3 Beta added a String caller parameter for debugging:
                // <https://android.googlesource.com/platform/frameworks/base/+/670fb7f5c0d23cf51ead25538bcb017e03ed73ac%5E%21/>
                freezeDisplayRotationMethod = manager.getClass().getMethod("freezeDisplayRotation", int.class, int.class, String.class);
                freezeDisplayRotationMethodVersion = 0;
            } catch (NoSuchMethodException e) {
                try {
                    // New method added by this commit:
                    // <https://android.googlesource.com/platform/frameworks/base/+/90c9005e687aa0f63f1ac391adc1e8878ab31759%5E%21/>
                    freezeDisplayRotationMethod = manager.getClass().getMethod("freezeDisplayRotation", int.class, int.class);
                    freezeDisplayRotationMethodVersion = 1;
                } catch (NoSuchMethodException e1) {
                    freezeDisplayRotationMethod = manager.getClass().getMethod("freezeRotation", int.class);
                    freezeDisplayRotationMethodVersion = 2;
                }
            }
        }
        return freezeDisplayRotationMethod;
    }

    private Method getIsDisplayRotationFrozenMethod() throws NoSuchMethodException {
        if (isDisplayRotationFrozenMethod == null) {
            try {
                // New method added by this commit:
                // <https://android.googlesource.com/platform/frameworks/base/+/90c9005e687aa0f63f1ac391adc1e8878ab31759%5E%21/>
                isDisplayRotationFrozenMethod = manager.getClass().getMethod("isDisplayRotationFrozen", int.class);
                isDisplayRotationFrozenMethodVersion = 0;
            } catch (NoSuchMethodException e) {
                isDisplayRotationFrozenMethod = manager.getClass().getMethod("isRotationFrozen");
                isDisplayRotationFrozenMethodVersion = 1;
            }
        }
        return isDisplayRotationFrozenMethod;
    }

    private Method getThawDisplayRotationMethod() throws NoSuchMethodException {
        if (thawDisplayRotationMethod == null) {
            try {
                // Android 15 preview and 14 QPR3 Beta added a String caller parameter for debugging:
                // <https://android.googlesource.com/platform/frameworks/base/+/670fb7f5c0d23cf51ead25538bcb017e03ed73ac%5E%21/>
                thawDisplayRotationMethod = manager.getClass().getMethod("thawDisplayRotation", int.class, String.class);
                thawDisplayRotationMethodVersion = 0;
            } catch (NoSuchMethodException e) {
                try {
                    // New method added by this commit:
                    // <https://android.googlesource.com/platform/frameworks/base/+/90c9005e687aa0f63f1ac391adc1e8878ab31759%5E%21/>
                    thawDisplayRotationMethod = manager.getClass().getMethod("thawDisplayRotation", int.class);
                    thawDisplayRotationMethodVersion = 1;
                } catch (NoSuchMethodException e1) {
                    thawDisplayRotationMethod = manager.getClass().getMethod("thawRotation");
                    thawDisplayRotationMethodVersion = 2;
                }
            }
        }
        return thawDisplayRotationMethod;
    }

    public int getRotation() {
        try {
            Method method = getGetRotationMethod();
            return (int) method.invoke(manager);
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
            return 0;
        }
    }

    public void freezeRotation(int displayId, int rotation) {
        try {
            Method method = getFreezeDisplayRotationMethod();
            switch (freezeDisplayRotationMethodVersion) {
                case 0:
                    method.invoke(manager, displayId, rotation, "scrcpy#freezeRotation");
                    break;
                case 1:
                    method.invoke(manager, displayId, rotation);
                    break;
                default:
                    if (displayId != 0) {
                        Ln.e("Secondary display rotation not supported on this device");
                        return;
                    }
                    method.invoke(manager, rotation);
                    break;
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public boolean isRotationFrozen(int displayId) {
        try {
            Method method = getIsDisplayRotationFrozenMethod();
            switch (isDisplayRotationFrozenMethodVersion) {
                case 0:
                    return (boolean) method.invoke(manager, displayId);
                default:
                    if (displayId != 0) {
                        Ln.e("Secondary display rotation not supported on this device");
                        return false;
                    }
                    return (boolean) method.invoke(manager);
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }

    public void thawRotation(int displayId) {
        try {
            Method method = getThawDisplayRotationMethod();
            switch (thawDisplayRotationMethodVersion) {
                case 0:
                    method.invoke(manager, displayId, "scrcpy#thawRotation");
                    break;
                case 1:
                    method.invoke(manager, displayId);
                    break;
                default:
                    if (displayId != 0) {
                        Ln.e("Secondary display rotation not supported on this device");
                        return;
                    }
                    method.invoke(manager);
                    break;
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    @TargetApi(AndroidVersions.API_30_ANDROID_11)
    public int[] registerDisplayWindowListener(IDisplayWindowListener listener) {
        try {
            return (int[]) manager.getClass().getMethod("registerDisplayWindowListener", IDisplayWindowListener.class).invoke(manager, listener);
        } catch (Exception e) {
            Ln.e("Could not register display window listener", e);
        }
        return null;
    }

    @TargetApi(AndroidVersions.API_30_ANDROID_11)
    public void unregisterDisplayWindowListener(IDisplayWindowListener listener) {
        try {
            manager.getClass().getMethod("unregisterDisplayWindowListener", IDisplayWindowListener.class).invoke(manager, listener);
        } catch (Exception e) {
            Ln.e("Could not unregister display window listener", e);
        }
    }

    @TargetApi(AndroidVersions.API_29_ANDROID_10)
    private Method getGetDisplayImePolicyMethod() throws NoSuchMethodException {
        if (getDisplayImePolicyMethod == null) {
            if (Build.VERSION.SDK_INT >= AndroidVersions.API_31_ANDROID_12) {
                getDisplayImePolicyMethod = manager.getClass().getMethod("getDisplayImePolicy", int.class);
            } else {
                getDisplayImePolicyMethod = manager.getClass().getMethod("shouldShowIme", int.class);
            }
        }
        return getDisplayImePolicyMethod;
    }

    @TargetApi(AndroidVersions.API_29_ANDROID_10)
    public int getDisplayImePolicy(int displayId) {
        try {
            Method method = getGetDisplayImePolicyMethod();
            if (Build.VERSION.SDK_INT >= AndroidVersions.API_31_ANDROID_12) {
                return (int) method.invoke(manager, displayId);
            }
            boolean shouldShowIme = (boolean) method.invoke(manager, displayId);
            return shouldShowIme ? DISPLAY_IME_POLICY_LOCAL : DISPLAY_IME_POLICY_FALLBACK_DISPLAY;
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
            return -1;
        }
    }

    @TargetApi(AndroidVersions.API_29_ANDROID_10)
    private Method getSetDisplayImePolicyMethod() throws NoSuchMethodException {
        if (setDisplayImePolicyMethod == null) {
            if (Build.VERSION.SDK_INT >= AndroidVersions.API_31_ANDROID_12) {
                setDisplayImePolicyMethod = manager.getClass().getMethod("setDisplayImePolicy", int.class, int.class);
            } else {
                setDisplayImePolicyMethod = manager.getClass().getMethod("setShouldShowIme", int.class, boolean.class);
            }
        }
        return setDisplayImePolicyMethod;
    }

    @TargetApi(AndroidVersions.API_29_ANDROID_10)
    public void setDisplayImePolicy(int displayId, int displayImePolicy) {
        try {
            Method method = getSetDisplayImePolicyMethod();
            if (Build.VERSION.SDK_INT >= AndroidVersions.API_31_ANDROID_12) {
                method.invoke(manager, displayId, displayImePolicy);
            } else if (displayImePolicy != DISPLAY_IME_POLICY_HIDE) {
                method.invoke(manager, displayId, displayImePolicy == DISPLAY_IME_POLICY_LOCAL);
            } else {
                Ln.w("DISPLAY_IME_POLICY_HIDE is not supported before Android 12");
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    /**
     * Set the windowing mode of a display's default task area via WindowContainerTransaction.
     *
     * This is required for Android desktop mode to operate in freeform windowing mode on virtual displays.
     */
    @TargetApi(AndroidVersions.API_34_ANDROID_14)
    @SuppressWarnings("unchecked")
    public void setDisplayWindowingMode(int displayId, int windowingMode) {
        // A Binder token with the correct AIDL interface descriptor, so that any system server
        // callbacks during registration are properly identified and silently handled
        IBinder organizerBinder = new Binder() {
            {
                attachInterface(null, "android.window.IDisplayAreaOrganizer");
            }

            @Override
            protected boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags)
                    throws android.os.RemoteException {
                if (super.onTransact(code, data, reply, flags)) {
                    return true;
                }
                // Silently accept any organizer callback transactions
                return true;
            }
        };
        Object organizerProxy = null;
        Object daoController = null;
        try {
            // Get the IWindowOrganizerController via ServiceManager
            IInterface windowOrganizerController = ServiceManager.getService("window_organizer",
                    "android.window.IWindowOrganizerController");
            if (windowOrganizerController == null) {
                Ln.w("window_organizer service not available");
                return;
            }

            // Get the IDisplayAreaOrganizerController
            daoController = windowOrganizerController.getClass().getMethod("getDisplayAreaOrganizerController").invoke(windowOrganizerController);

            // Create a no-op IDisplayAreaOrganizer proxy for registration; callbacks are ignored since we unregister immediately
            Class<?> idaoClass = Class.forName("android.window.IDisplayAreaOrganizer");
            organizerProxy = Proxy.newProxyInstance(
                    ClassLoader.getSystemClassLoader(),
                    new Class[]{idaoClass},
                    (proxy, method, args) -> {
                        if ("asBinder".equals(method.getName())) {
                            return organizerBinder;
                        }
                        return null;
                    }
            );

            // Register the organizer to get display area tokens
            Object parceledList = daoController.getClass()
                    .getMethod("registerOrganizer", idaoClass, int.class)
                    .invoke(daoController, organizerProxy, FEATURE_DEFAULT_TASK_CONTAINER);

            List<Object> displayAreaInfos = (List<Object>) parceledList.getClass().getMethod("getList").invoke(parceledList);

            // Find the display area token matching our virtual display
            Object targetToken = null;
            for (Object info : displayAreaInfos) {
                Object displayAreaInfo = info.getClass().getMethod("getDisplayAreaInfo").invoke(info);
                int daDisplayId = displayAreaInfo.getClass().getDeclaredField("displayId").getInt(displayAreaInfo);
                if (daDisplayId == displayId) {
                    targetToken = displayAreaInfo.getClass().getDeclaredField("token").get(displayAreaInfo);
                    break;
                }
            }

            if (targetToken != null) {
                // Create a WindowContainerTransaction to set the windowing mode
                Class<?> wctClass = Class.forName("android.window.WindowContainerTransaction");
                Object wct = wctClass.getDeclaredConstructor().newInstance();

                Class<?> tokenClass = Class.forName("android.window.WindowContainerToken");
                wctClass.getMethod("setWindowingMode", tokenClass, int.class).invoke(wct, targetToken, windowingMode);

                // Apply the transaction
                windowOrganizerController.getClass().getMethod("applyTransaction", wctClass).invoke(windowOrganizerController, wct);
            } else {
                Ln.w("Could not find display area for display " + displayId);
            }
        } catch (Exception e) {
            Ln.w("Could not set windowing mode for display " + displayId, e);
        } finally {
            // Always unregister the organizer
            if (organizerProxy != null && daoController != null) {
                try {
                    Class<?> idaoClass = Class.forName("android.window.IDisplayAreaOrganizer");
                    daoController.getClass().getMethod("unregisterOrganizer", idaoClass).invoke(daoController, organizerProxy);
                } catch (Exception e) {
                    Ln.w("Could not unregister display area organizer", e);
                }
            }
        }
    }
}
