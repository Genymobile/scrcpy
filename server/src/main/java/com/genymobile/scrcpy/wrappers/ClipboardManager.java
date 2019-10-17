package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.content.ClipData;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ClipboardManager {
    private final IInterface manager;
    private Method getPrimaryClipMethod;
    private Method setPrimaryClipMethod;

    public ClipboardManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getGetPrimaryClipMethod() {
        if (getPrimaryClipMethod == null) {
            try {
                getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class);
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return getPrimaryClipMethod;
    }

    private Method getSetPrimaryClipMethod() {
        if (setPrimaryClipMethod == null) {
            try {
                setPrimaryClipMethod = manager.getClass().getMethod("setPrimaryClip", ClipData.class, String.class);
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return setPrimaryClipMethod;
    }

    public CharSequence getText() {
        Method method = getGetPrimaryClipMethod();
        if (method == null) {
            return null;
        }
        try {
            ClipData clipData = (ClipData) method.invoke(manager, "com.android.shell");
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
            method.invoke(manager, clipData, "com.android.shell");
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Could not invoke " + method.getName(), e);
        }
    }
}
