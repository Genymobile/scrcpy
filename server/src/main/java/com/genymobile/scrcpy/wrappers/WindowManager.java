package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.os.IInterface;
import android.view.IRotationWatcher;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public final class WindowManager {
    private final IInterface manager;
    private Method getRotationMethod;
    private Method freezeRotationMethod;
    private Method isRotationFrozenMethod;
    private Method thawRotationMethod;

    public WindowManager(IInterface manager) {
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

    private Method getFreezeRotationMethod() throws NoSuchMethodException {
        if (freezeRotationMethod == null) {
            freezeRotationMethod = manager.getClass().getMethod("freezeRotation", int.class);
        }
        return freezeRotationMethod;
    }

    private Method getIsRotationFrozenMethod() throws NoSuchMethodException {
        if (isRotationFrozenMethod == null) {
            isRotationFrozenMethod = manager.getClass().getMethod("isRotationFrozen");
        }
        return isRotationFrozenMethod;
    }

    private Method getThawRotationMethod() throws NoSuchMethodException {
        if (thawRotationMethod == null) {
            thawRotationMethod = manager.getClass().getMethod("thawRotation");
        }
        return thawRotationMethod;
    }

    public int getRotation() {
        try {
            Method method = getGetRotationMethod();
            return (int) method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return 0;
        }
    }

    public void freezeRotation(int rotation) {
        try {
            Method method = getFreezeRotationMethod();
            method.invoke(manager, rotation);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public boolean isRotationFrozen() {
        try {
            Method method = getIsRotationFrozenMethod();
            return (boolean) method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }

    public void thawRotation() {
        try {
            Method method = getThawRotationMethod();
            method.invoke(manager);
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public void registerRotationWatcher(IRotationWatcher rotationWatcher, int displayId) {
        try {
            Class<?> cls = manager.getClass();
            try {
                // display parameter added since this commit:
                // https://android.googlesource.com/platform/frameworks/base/+/35fa3c26adcb5f6577849fd0df5228b1f67cf2c6%5E%21/#F1
                cls.getMethod("watchRotation", IRotationWatcher.class, int.class).invoke(manager, rotationWatcher, displayId);
            } catch (NoSuchMethodException e) {
                // old version
                cls.getMethod("watchRotation", IRotationWatcher.class).invoke(manager, rotationWatcher);
            }
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }
}
