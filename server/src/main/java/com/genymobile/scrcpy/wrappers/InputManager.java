package com.genymobile.scrcpy.wrappers;

import com.genymobile.scrcpy.Ln;

import android.os.IInterface;
import android.view.InputEvent;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public final class InputManager {

    public static final int INJECT_INPUT_EVENT_MODE_ASYNC = 0;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT = 1;
    public static final int INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH = 2;

    private final IInterface manager;
    private Method injectInputEventMethod;

    public InputManager(IInterface manager) {
        this.manager = manager;
    }

    private Method getInjectInputEventMethod() {
        if (injectInputEventMethod == null) {
            try {
                injectInputEventMethod = manager.getClass().getMethod("injectInputEvent", InputEvent.class, int.class);
            } catch (NoSuchMethodException e) {
                Ln.e("Could not find method", e);
            }
        }
        return injectInputEventMethod;
    }

    public boolean injectInputEvent(InputEvent inputEvent, int mode) {
        Method method = getInjectInputEventMethod();
        if (method == null) {
            return false;
        }
        try {
            return (Boolean) method.invoke(manager, inputEvent, mode);
        } catch (InvocationTargetException | IllegalAccessException e) {
            Ln.e("Could not invoke " + method.getName(), e);
            return false;
        }
    }
}
