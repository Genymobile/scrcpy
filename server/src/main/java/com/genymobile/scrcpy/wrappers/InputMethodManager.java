package com.genymobile.scrcpy.wrappers;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import android.os.IInterface;
import com.android.internal.view.IInputMethodClient;
import com.genymobile.scrcpy.Ln;

public final class InputMethodManager {
    private final IInterface manager;
    private final Method showInputMethodPickerMethod;
    private final IInputMethodClient.Stub stub = new IInputMethodClient.Stub() {};

    public InputMethodManager(IInterface manager) {
        this.manager = manager;
        try {
            for(Field field : manager.getClass().getDeclaredFields()) {
                Ln.i("field:" + field.getName());
            }
            showInputMethodPickerMethod = manager.getClass().getMethod("showInputMethodPickerFromClient", IInputMethodClient.class, int.class);
        } catch (NoSuchMethodException e) {
            throw new AssertionError(e);
        }
    }

    public void showInputMethodPicker() {
        try {
            showInputMethodPickerMethod.invoke(manager, stub, 0);
        } catch (IllegalAccessException | InvocationTargetException e) {
            throw new AssertionError(e);
        }
    }
}
