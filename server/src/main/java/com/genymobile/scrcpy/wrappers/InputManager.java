package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;

import android.annotation.SuppressLint;
import android.view.InputEvent;
import android.view.MotionEvent;

import java.lang.reflect.Method;

@SuppressLint("PrivateApi,DiscouragedPrivateApi")
public final class InputManager {

    public static final int INJECT_INPUT_EVENT_MODE_ASYNC = 0;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT = 1;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH = 2;

    private final android.hardware.input.InputManager manager;
    private static Method injectInputEventMethod;
    private static Method setDisplayIdMethod;
    private static Method setActionButtonMethod;

    static InputManager create() {
        android.hardware.input.InputManager manager = (android.hardware.input.InputManager) FakeContext.get()
                .getSystemService(FakeContext.INPUT_SERVICE);
        return new InputManager(manager);
    }

    private InputManager(android.hardware.input.InputManager manager) {
        this.manager = manager;
    }

    private Method getInjectInputEventMethod() throws NoSuchMethodException {
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
}
