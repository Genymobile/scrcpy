package com.genymobile.scrcpy.wrappers;

import android.os.IBinder;
import android.os.IInterface;

import java.lang.reflect.Method;

public class ServiceManager {
    private final Method getServiceMethod;

    public ServiceManager() {
        try {
            getServiceMethod = Class.forName("android.os.ServiceManager").getDeclaredMethod("getService", String.class);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    private IInterface getService(String service, String type) {
        try {
            IBinder binder = (IBinder) getServiceMethod.invoke(null, service);
            Method asInterfaceMethod = Class.forName(type + "$Stub").getMethod("asInterface", IBinder.class);
            return (IInterface) asInterfaceMethod.invoke(null, binder);
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    public WindowManager getWindowManager() {
        return new WindowManager(getService("window", "android.view.IWindowManager"));
    }

    public DisplayManager getDisplayManager() {
        return new DisplayManager(getService("display", "android.hardware.display.IDisplayManager"));
    }

    public InputManager getInputManager() {
        return new InputManager(getService("input", "android.hardware.input.IInputManager"));
    }

    public PowerManager getPowerManager() {
        return new PowerManager(getService("power", "android.os.IPowerManager"));
    }
}
