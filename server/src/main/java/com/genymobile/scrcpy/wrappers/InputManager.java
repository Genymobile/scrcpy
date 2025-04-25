package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.InputEvent;
import android.view.MotionEvent;

import java.lang.reflect.Method;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class InputManager {

    public static final int INJECT_INPUT_EVENT_MODE_ASYNC = 0;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT = 1;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH = 2;

    private final Object manager;

    private static Method injectInputEventMethod;
    private static Method setDisplayIdMethod;
    private static Method setActionButtonMethod;
    private static Method addUniqueIdAssociationByPortMethod;
    private static Method removeUniqueIdAssociationByPortMethod;

    static InputManager create() {
        Object im = FakeContext.get().getSystemService(Context.INPUT_SERVICE);
        return new InputManager(im);
    }

    private InputManager(Object manager) {
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
            addUniqueIdAssociationByPortMethod = android.hardware.input.InputManager.class.getMethod("addUniqueIdAssociationByPort", String.class, String.class);
        }
        return addUniqueIdAssociationByPortMethod;
    }

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
            removeUniqueIdAssociationByPortMethod = android.hardware.input.InputManager.class.getMethod("removeUniqueIdAssociationByPort", String.class);
        }
        return removeUniqueIdAssociationByPortMethod;
    }

    public void removeUniqueIdAssociationByPort(String inputPort) {
        try {
            Method method = getRemoveUniqueIdAssociationByPortMethod();
            method.invoke(manager, inputPort);
        } catch (ReflectiveOperationException e) {
            Ln.e("Cannot remove unique id association by port", e);
        }
    }
}
