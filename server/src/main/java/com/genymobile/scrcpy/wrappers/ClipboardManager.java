package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.content.ClipData;
import android.content.IOnPrimaryClipChangedListener;
import android.os.Build;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ClipboardManager {
    private final IInterface manager;
    private Method getPrimaryClipMethod;
    private Method setPrimaryClipMethod;
    private Method addPrimaryClipChangedListener;
    private boolean alternativeGetMethod;
    private boolean alternativeSetMethod;
    private boolean alternativeAddListenerMethod;

    public ClipboardManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getGetPrimaryClipMethod() throws NoSuchMethodException {
        if (getPrimaryClipMethod == null) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class);
            } else {
                try {
                    getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class, int.class);
                } catch (NoSuchMethodException e) {
                    getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class, String.class, int.class);
                    alternativeGetMethod = true;
                }
            }
        }
        return getPrimaryClipMethod;
    }

    private Method getSetPrimaryClipMethod() throws NoSuchMethodException {
        if (setPrimaryClipMethod == null) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                setPrimaryClipMethod = manager.getClass().getMethod("setPrimaryClip", ClipData.class, String.class);
            } else {
                try {
                    setPrimaryClipMethod = manager.getClass().getMethod("setPrimaryClip", ClipData.class, String.class, int.class);
                } catch (NoSuchMethodException e) {
                    setPrimaryClipMethod = manager.getClass().getMethod("setPrimaryClip", ClipData.class, String.class, String.class, int.class);
                    alternativeSetMethod = true;
                }
            }
        }
        return setPrimaryClipMethod;
    }

    private static ClipData getPrimaryClip(Method method, boolean alternativeMethod, IInterface manager)
            throws InvocationTargetException, IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return (ClipData) method.invoke(manager, ServiceManager.PACKAGE_NAME);
        }
        if (alternativeMethod) {
            return (ClipData) method.invoke(manager, ServiceManager.PACKAGE_NAME, null, ServiceManager.USER_ID);
        }
        return (ClipData) method.invoke(manager, ServiceManager.PACKAGE_NAME, ServiceManager.USER_ID);
    }

    private static void setPrimaryClip(Method method, boolean alternativeMethod, IInterface manager, ClipData clipData)
            throws InvocationTargetException, IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            method.invoke(manager, clipData, ServiceManager.PACKAGE_NAME);
        } else if (alternativeMethod) {
            method.invoke(manager, clipData, ServiceManager.PACKAGE_NAME, null, ServiceManager.USER_ID);
        } else {
            method.invoke(manager, clipData, ServiceManager.PACKAGE_NAME, ServiceManager.USER_ID);
        }
    }

    public CharSequence getText() {
        try {
            Method method = getGetPrimaryClipMethod();
            ClipData clipData = getPrimaryClip(method, alternativeGetMethod, manager);
            if (clipData == null || clipData.getItemCount() == 0) {
                return null;
            }
            return clipData.getItemAt(0).getText();
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return null;
        }
    }

    public boolean setText(CharSequence text) {
        try {
            Method method = getSetPrimaryClipMethod();
            ClipData clipData = ClipData.newPlainText(null, text);
            setPrimaryClip(method, alternativeSetMethod, manager, clipData);
            return true;
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }

    private static void addPrimaryClipChangedListener(Method method, boolean alternativeMethod, IInterface manager,
            IOnPrimaryClipChangedListener listener) throws InvocationTargetException, IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            method.invoke(manager, listener, ServiceManager.PACKAGE_NAME);
        } else if (alternativeMethod) {
            method.invoke(manager, listener, ServiceManager.PACKAGE_NAME, null, ServiceManager.USER_ID);
        } else {
            method.invoke(manager, listener, ServiceManager.PACKAGE_NAME, ServiceManager.USER_ID);
        }
    }

    private Method getAddPrimaryClipChangedListener() throws NoSuchMethodException {
        if (addPrimaryClipChangedListener == null) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                addPrimaryClipChangedListener = manager.getClass()
                        .getMethod("addPrimaryClipChangedListener", IOnPrimaryClipChangedListener.class, String.class);
            } else {
                try {
                    addPrimaryClipChangedListener = manager.getClass()
                            .getMethod("addPrimaryClipChangedListener", IOnPrimaryClipChangedListener.class, String.class, int.class);
                } catch (NoSuchMethodException e) {
                    addPrimaryClipChangedListener = manager.getClass()
                            .getMethod("addPrimaryClipChangedListener", IOnPrimaryClipChangedListener.class, String.class, String.class, int.class);
                    alternativeAddListenerMethod = true;
                }
            }
        }
        return addPrimaryClipChangedListener;
    }

    public boolean addPrimaryClipChangedListener(IOnPrimaryClipChangedListener listener) {
        try {
            Method method = getAddPrimaryClipChangedListener();
            addPrimaryClipChangedListener(method, alternativeAddListenerMethod, manager, listener);
            return true;
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }
}
