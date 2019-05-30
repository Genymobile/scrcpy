package com.genymobile.scrcpy.wrappers;

import android.content.ClipData;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class ClipboardManager {
    private final IInterface manager;
    private final Method getPrimaryClipMethod;

    public ClipboardManager(IInterface manager) {
        this.manager = manager;
        try {
            getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class);
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        }
    }

    public CharSequence getText() {
        try {
            ClipData clipData = (ClipData) getPrimaryClipMethod.invoke(manager, "com.android.shell");
            if (clipData == null || clipData.getItemCount() == 0) {
                return null;
            }
            return clipData.getItemAt(0).getText();
        } catch (InvocationTargetException | IllegalAccessException e) {
            throw new AssertionError(e);
        }
    }
}
