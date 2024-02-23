package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.annotation.TargetApi;
import android.os.IInterface;
import android.view.IDisplayFoldListener;
import android.view.IRotationWatcher;

import java.lang.reflect.Method;

public final class WindowManager {
    private final IInterface manager;
    private Method getRotationMethod;
    private Method freezeRotationMethod;
    private Method freezeDisplayRotationMethod;
    private Method isRotationFrozenMethod;
    private Method isDisplayRotationFrozenMethod;
    private Method thawRotationMethod;
    private Method thawDisplayRotationMethod;

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

    private Method getFreezeRotationMethod() throws NoSuchMethodException {
        if (freezeRotationMethod == null) {
            freezeRotationMethod = manager.getClass().getMethod("freezeRotation", int.class);
        }
        return freezeRotationMethod;
    }

    // New method added by this commit:
    // <https://android.googlesource.com/platform/frameworks/base/+/90c9005e687aa0f63f1ac391adc1e8878ab31759%5E%21/>
    private Method getFreezeDisplayRotationMethod() throws NoSuchMethodException {
        if (freezeDisplayRotationMethod == null) {
            freezeDisplayRotationMethod = manager.getClass().getMethod("freezeDisplayRotation", int.class, int.class);
        }
        return freezeDisplayRotationMethod;
    }

    private Method getIsRotationFrozenMethod() throws NoSuchMethodException {
        if (isRotationFrozenMethod == null) {
            isRotationFrozenMethod = manager.getClass().getMethod("isRotationFrozen");
        }
        return isRotationFrozenMethod;
    }

    // New method added by this commit:
    // <https://android.googlesource.com/platform/frameworks/base/+/90c9005e687aa0f63f1ac391adc1e8878ab31759%5E%21/>
    private Method getIsDisplayRotationFrozenMethod() throws NoSuchMethodException {
        if (isDisplayRotationFrozenMethod == null) {
            isDisplayRotationFrozenMethod = manager.getClass().getMethod("isDisplayRotationFrozen", int.class);
        }
        return isDisplayRotationFrozenMethod;
    }

    private Method getThawRotationMethod() throws NoSuchMethodException {
        if (thawRotationMethod == null) {
            thawRotationMethod = manager.getClass().getMethod("thawRotation");
        }
        return thawRotationMethod;
    }

    // New method added by this commit:
    // <https://android.googlesource.com/platform/frameworks/base/+/90c9005e687aa0f63f1ac391adc1e8878ab31759%5E%21/>
    private Method getThawDisplayRotationMethod() throws NoSuchMethodException {
        if (thawDisplayRotationMethod == null) {
            thawDisplayRotationMethod = manager.getClass().getMethod("thawDisplayRotation", int.class);
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
            try {
                Method method = getFreezeDisplayRotationMethod();
                method.invoke(manager, displayId, rotation);
            } catch (ReflectiveOperationException e) {
                if (displayId == 0) {
                    Method method = getFreezeRotationMethod();
                    method.invoke(manager, rotation);
                } else {
                    Ln.e("Could not invoke method", e);
                }
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
        }
    }

    public boolean isRotationFrozen(int displayId) {
        try {
            try {
                Method method = getIsDisplayRotationFrozenMethod();
                return (boolean) method.invoke(manager, displayId);
            } catch (ReflectiveOperationException e) {
                if (displayId == 0) {
                    Method method = getIsRotationFrozenMethod();
                    return (boolean) method.invoke(manager);
                } else {
                    Ln.e("Could not invoke method", e);
                    return false;
                }
            }
        } catch (ReflectiveOperationException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }

    public void thawRotation(int displayId) {
        try {
            try {
                Method method = getThawDisplayRotationMethod();
                method.invoke(manager, displayId);
            } catch (ReflectiveOperationException e) {
                if (displayId == 0) {
                    Method method = getThawRotationMethod();
                    method.invoke(manager);
                } else {
                    Ln.e("Could not invoke method", e);
                }
            }
        } catch (ReflectiveOperationException e) {
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
            Ln.e("Could not register rotation watcher", e);
        }
    }

    @TargetApi(29)
    public void registerDisplayFoldListener(IDisplayFoldListener foldListener) {
        try {
            Class<?> cls = manager.getClass();
            cls.getMethod("registerDisplayFoldListener", IDisplayFoldListener.class).invoke(manager, foldListener);
        } catch (Exception e) {
            Ln.e("Could not register display fold listener", e);
        }
    }
}
