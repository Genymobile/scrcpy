package com.genymobile.scrcpy.wrappers;

import android.os.IInterface;
import android.view.IRotationWatcher;

public final class WindowManager {
    private final IInterface manager;

    public WindowManager(IInterface manager) {
        this.manager = manager;
    }

    public int getRotation() {
        try {
            Class<?> cls = manager.getClass();
            try {
                return (Integer) manager.getClass().getMethod("getRotation").invoke(manager);
            } catch (NoSuchMethodException e) {
                // method changed since this commit:
                // https://android.googlesource.com/platform/frameworks/base/+/8ee7285128c3843401d4c4d0412cd66e86ba49e3%5E%21/#F2
                return (Integer) cls.getMethod("getDefaultDisplayRotation").invoke(manager);
            }
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public void registerRotationWatcher(IRotationWatcher rotationWatcher) {
        try {
            Class<?> cls = manager.getClass();
            try {
                cls.getMethod("watchRotation", IRotationWatcher.class).invoke(manager, rotationWatcher);
            } catch (NoSuchMethodException e) {
                // display parameter added since this commit:
                // https://android.googlesource.com/platform/frameworks/base/+/35fa3c26adcb5f6577849fd0df5228b1f67cf2c6%5E%21/#F1
                cls.getMethod("watchRotation", IRotationWatcher.class, int.class).invoke(manager, rotationWatcher, 0);
            }
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }
}
