package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public final class PowerManager {
    private final IInterface manager;
    private Method isScreenOnMethod;

    public PowerManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getIsScreenOnMethod() {
        if (isScreenOnMethod == null) {
            try {
                @SuppressLint("ObsoleteSdkInt") // we may lower minSdkVersion in the future
                        String methodName = Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH ? "isInteractive" : "isScreenOn";
                isScreenOnMethod = manager.getClass().getMethod(methodName);
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return isScreenOnMethod;
    }

    public boolean isScreenOn() {
        Method method = getIsScreenOnMethod();
        if (method == null) {
            return false;
        }
        try {
            return (Boolean) method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Could not invoke " + method.getName(), e);
            return false;
        }
    }
}
