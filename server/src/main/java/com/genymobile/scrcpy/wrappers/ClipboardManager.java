package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.content.ClipData;
import android.os.Build;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ClipboardManager {

    private static final String PACKAGE_NAME = "com.android.shell";
    private static final int USER_ID = 0;

    private final IInterface manager;
    private Method getPrimaryClipMethod;
    private Method setPrimaryClipMethod;

    public ClipboardManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getGetPrimaryClipMethod() {
        if (getPrimaryClipMethod == null) {
            try {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                    getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class);
                } else {
                    getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class, int.class);
                }
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return getPrimaryClipMethod;
    }

    private Method getSetPrimaryClipMethod() {
        if (setPrimaryClipMethod == null) {
            try {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                    setPrimaryClipMethod = manager.getClass().getMethod("setPrimaryClip", ClipData.class, String.class);
                } else {
                    setPrimaryClipMethod = manager.getClass().getMethod("setPrimaryClip", ClipData.class,
                            String.class, int.class);
                }
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return setPrimaryClipMethod;
    }

    private static ClipData getPrimaryClip(Method method, IInterface manager) throws InvocationTargetException,
            IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return (ClipData) method.invoke(manager, PACKAGE_NAME);
        }
        return (ClipData) method.invoke(manager, PACKAGE_NAME, USER_ID);
    }

    private static void setPrimaryClip(Method method, IInterface manager, ClipData clipData) throws InvocationTargetException,
            IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            method.invoke(manager, clipData, PACKAGE_NAME);
        } else {
            method.invoke(manager, clipData, PACKAGE_NAME, USER_ID);
        }
    }

    public CharSequence getText() {
        Method method = getGetPrimaryClipMethod();
        if (method == null) {
            return null;
        }
        try {
            ClipData clipData = getPrimaryClip(method, manager);
            if (clipData == null || clipData.getItemCount() == 0) {
                return null;
            }
            return clipData.getItemAt(0).getText();
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Could not invoke " + method.getName(), e);
            return null;
        }
    }

    public void setText(CharSequence text) {
        Method method = getSetPrimaryClipMethod();
        if (method == null) {
            return;
        }
        ClipData clipData = ClipData.newPlainText(null, text);
        try {
            setPrimaryClip(method, manager, clipData);
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Could not invoke " + method.getName(), e);
        }
    }
}
