package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.Ln;

import android.content.ClipData;
import android.content.IOnPrimaryClipChangedListener;
import android.os.Build;
import android.os.IInterface;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public final class ClipboardManager {
    private final IInterface manager;
    private Method getPrimaryClipMethod;
    private Method setPrimaryClipMethod;
    private Method addPrimaryClipChangedListener;
    private int getMethodVersion;
    private int setMethodVersion;
    private int addListenerMethodVersion;

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
                    getMethodVersion = 0;
                } catch (NoSuchMethodException e1) {
                    try {
                        getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class, String.class, int.class);
                        getMethodVersion = 1;
                    } catch (NoSuchMethodException e2) {
                        try {
                            getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class, String.class, int.class, int.class);
                            getMethodVersion = 2;
                        } catch (NoSuchMethodException e3) {
                            getPrimaryClipMethod = manager.getClass().getMethod("getPrimaryClip", String.class, int.class, String.class);
                            getMethodVersion = 3;
                        }
                    }
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
                    setMethodVersion = 0;
                } catch (NoSuchMethodException e1) {
                    try {
                        setPrimaryClipMethod = manager.getClass().getMethod("setPrimaryClip", ClipData.class, String.class, String.class, int.class);
                        setMethodVersion = 1;
                    } catch (NoSuchMethodException e2) {
                        setPrimaryClipMethod = manager.getClass()
                                .getMethod("setPrimaryClip", ClipData.class, String.class, String.class, int.class, int.class);
                        setMethodVersion = 2;
                    }
                }
            }
        }
        return setPrimaryClipMethod;
    }

    private static ClipData getPrimaryClip(Method method, int methodVersion, IInterface manager)
            throws InvocationTargetException, IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return (ClipData) method.invoke(manager, FakeContext.PACKAGE_NAME);
        }

        switch (methodVersion) {
            case 0:
                return (ClipData) method.invoke(manager, FakeContext.PACKAGE_NAME, FakeContext.ROOT_UID);
            case 1:
                return (ClipData) method.invoke(manager, FakeContext.PACKAGE_NAME, null, FakeContext.ROOT_UID);
            case 2:
                return (ClipData) method.invoke(manager, FakeContext.PACKAGE_NAME, null, FakeContext.ROOT_UID, 0);
            default:
                return (ClipData) method.invoke(manager, FakeContext.PACKAGE_NAME, FakeContext.ROOT_UID, null);
        }
    }

    private static void setPrimaryClip(Method method, int methodVersion, IInterface manager, ClipData clipData)
            throws InvocationTargetException, IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            method.invoke(manager, clipData, FakeContext.PACKAGE_NAME);
            return;
        }

        switch (methodVersion) {
            case 0:
                method.invoke(manager, clipData, FakeContext.PACKAGE_NAME, FakeContext.ROOT_UID);
                break;
            case 1:
                method.invoke(manager, clipData, FakeContext.PACKAGE_NAME, null, FakeContext.ROOT_UID);
                break;
            default:
                method.invoke(manager, clipData, FakeContext.PACKAGE_NAME, null, FakeContext.ROOT_UID, 0);
                break;
        }
    }

    public CharSequence getText() {
        try {
            Method method = getGetPrimaryClipMethod();
            ClipData clipData = getPrimaryClip(method, getMethodVersion, manager);
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
            setPrimaryClip(method, setMethodVersion, manager, clipData);
            return true;
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }

    private static void addPrimaryClipChangedListener(Method method, int methodVersion, IInterface manager, IOnPrimaryClipChangedListener listener)
            throws InvocationTargetException, IllegalAccessException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            method.invoke(manager, listener, FakeContext.PACKAGE_NAME);
            return;
        }

        switch (methodVersion) {
            case 0:
                method.invoke(manager, listener, FakeContext.PACKAGE_NAME, FakeContext.ROOT_UID);
                break;
            case 1:
                method.invoke(manager, listener, FakeContext.PACKAGE_NAME, null, FakeContext.ROOT_UID);
                break;
            default:
                method.invoke(manager, listener, FakeContext.PACKAGE_NAME, null, FakeContext.ROOT_UID, 0);
                break;
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
                    addListenerMethodVersion = 0;
                } catch (NoSuchMethodException e1) {
                    try {
                        addPrimaryClipChangedListener = manager.getClass()
                                .getMethod("addPrimaryClipChangedListener", IOnPrimaryClipChangedListener.class, String.class, String.class,
                                        int.class);
                        addListenerMethodVersion = 1;
                    } catch (NoSuchMethodException e2) {
                        addPrimaryClipChangedListener = manager.getClass()
                                .getMethod("addPrimaryClipChangedListener", IOnPrimaryClipChangedListener.class, String.class, String.class,
                                        int.class, int.class);
                        addListenerMethodVersion = 2;
                    }
                }
            }
        }
        return addPrimaryClipChangedListener;
    }

    public boolean addPrimaryClipChangedListener(IOnPrimaryClipChangedListener listener) {
        try {
            Method method = getAddPrimaryClipChangedListener();
            addPrimaryClipChangedListener(method, addListenerMethodVersion, manager, listener);
            return true;
        } catch (InvocationTargetException | IllegalAccessException | NoSuchMethodException e) {
            Ln.e("Could not invoke method", e);
            return false;
        }
    }
}
