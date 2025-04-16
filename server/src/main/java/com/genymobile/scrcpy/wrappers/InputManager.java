package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.view.InputEvent;
import android.view.MotionEvent;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class InputManager {

    public static final int INJECT_INPUT_EVENT_MODE_ASYNC = 0;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT = 1;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH = 2;

    private final android.hardware.input.InputManager manager;
    private long lastPermissionLogDate;

    private static Method injectInputEventMethod;
    private static Method setDisplayIdMethod;
    private static Method setActionButtonMethod;
    private static Method addUniqueIdAssociationByPortMethod;
    private static Method removeUniqueIdAssociationByPortMethod;

    static InputManager create() {
        android.hardware.input.InputManager manager = (android.hardware.input.InputManager) FakeContext.get()
                .getSystemService(FakeContext.INPUT_SERVICE);
        return new InputManager(manager);
    }

    private InputManager(android.hardware.input.InputManager manager) {
        this.manager = manager;
    }

    private static Method getInjectInputEventMethod() throws NoSuchMethodException {
        if (injectInputEventMethod == null) {
            injectInputEventMethod = android.hardware.input.InputManager.class.getMethod("injectInputEvent", InputEvent.class, int.class);
        }
        return injectInputEventMethod;
    }

    public boolean injectInputEvent(InputEvent inputEvent, int mode) {
        try {
            Method method = getInjectInputEventMethod();
            return (boolean) method.invoke(manager, inputEvent, mode);
        } catch (ReflectiveOperationException e) {
            if (e instanceof InvocationTargetException) {
                Throwable cause = e.getCause();
                if (cause instanceof SecurityException) {
                    String message = e.getCause().getMessage();
                    if (message != null && message.contains("INJECT_EVENTS permission")) {
                        // Do not flood the console, limit to one permission error log every 3 seconds
                        long now = System.currentTimeMillis();
                        if (lastPermissionLogDate <= now - 3000) {
                            Ln.e(message);
                            Ln.e("Make sure you have enabled \"USB debugging (Security Settings)\" and then rebooted your device.");
                            lastPermissionLogDate = now;
                        }
                        // Do not print the stack trace
                        return false;
                    }
                }
            }
            Ln.e("Could not invoke method", e);
            return false;
        }
    }

    private static Method getSetDisplayIdMethod() throws NoSuchMethodException {
        if (setDisplayIdMethod == null) {
            setDisplayIdMethod = InputEvent.class.getMethod("setDisplayId", int.class);
        }
        return setDisplayIdMethod;
    }

    public static boolean setDisplayId(InputEvent inputEvent, int displayId) {
        try {
            Method method = getSetDisplayIdMethod();
            method.invoke(inputEvent, displayId);
            return true;
        } catch (ReflectiveOperationException e) {
            Ln.e("Cannot associate a display id to the input event", e);
            return false;
        }
    }

    private static Method getSetActionButtonMethod() throws NoSuchMethodException {
        if (setActionButtonMethod == null) {
            setActionButtonMethod = MotionEvent.class.getMethod("setActionButton", int.class);
        }
        return setActionButtonMethod;
    }

    public static boolean setActionButton(MotionEvent motionEvent, int actionButton) {
        try {
            Method method = getSetActionButtonMethod();
            method.invoke(motionEvent, actionButton);
            return true;
        } catch (ReflectiveOperationException e) {
            Ln.e("Cannot set action button on MotionEvent", e);
            return false;
        }
    }

    private static Method getAddUniqueIdAssociationByPortMethod() throws NoSuchMethodException {
        if (addUniqueIdAssociationByPortMethod == null) {
            addUniqueIdAssociationByPortMethod = android.hardware.input.InputManager.class.getMethod(
                    "addUniqueIdAssociationByPort", String.class, String.class);
        }
        return addUniqueIdAssociationByPortMethod;
    }

    @TargetApi(AndroidVersions.API_35_ANDROID_15)
    public void addUniqueIdAssociationByPort(String inputPort, String uniqueId) {
        try {
            Method method = getAddUniqueIdAssociationByPortMethod();
            method.invoke(manager, inputPort, uniqueId);
        } catch (ReflectiveOperationException e) {
            Ln.e("Cannot add unique id association by port", e);
        }
    }

    private static Method getRemoveUniqueIdAssociationByPortMethod() throws NoSuchMethodException {
        if (removeUniqueIdAssociationByPortMethod == null) {
            removeUniqueIdAssociationByPortMethod = android.hardware.input.InputManager.class.getMethod(
                    "removeUniqueIdAssociationByPort", String.class);
        }
        return removeUniqueIdAssociationByPortMethod;
    }

    @TargetApi(AndroidVersions.API_35_ANDROID_15)
    public void removeUniqueIdAssociationByPort(String inputPort) {
        try {
            Method method = getRemoveUniqueIdAssociationByPortMethod();
            method.invoke(manager, inputPort);
        } catch (ReflectiveOperationException e) {
            Ln.e("Cannot remove unique id association by port", e);
        }
    }
}
