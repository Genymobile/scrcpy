package com.genymobile.scrcpy.wrappers;

import android.os.Build;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class PowerManager {
    private final IInterface manager;
    private final Method isScreenOnMethod;

    public PowerManager(IInterface manager) {
        this.manager = manager;
        try {
            String methodName = Build.VERSION.SDK_INT >= 20 ? "isInteractive" : "isScreenOn";
            isScreenOnMethod = manager.getClass().getMethod(methodName);
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        }
    }

    public boolean isScreenOn() {
        try {
            return (Boolean) isScreenOnMethod.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            throw new AssertionError(e);
        }
    }
}
