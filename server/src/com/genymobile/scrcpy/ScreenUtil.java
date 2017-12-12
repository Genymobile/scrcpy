package com.genymobile.scrcpy;

import android.os.IBinder;
import android.os.IInterface;
import android.view.IRotationWatcher;

import java.lang.reflect.Method;

public class ScreenUtil {

    private static final ServiceManager serviceManager = new ServiceManager();

    public static ScreenInfo getScreenInfo() {
        return serviceManager.getDisplayManager().getScreenInfo();
    }

    public static void registerRotationWatcher(IRotationWatcher rotationWatcher) {
        serviceManager.getWindowManager().registerRotationWatcher(rotationWatcher);
    }

    private static class ServiceManager {
        private Method getServiceMethod;

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
    }

    private static class WindowManager {
        private IInterface manager;

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

    private static class DisplayManager {
        private IInterface manager;

        public DisplayManager(IInterface manager) {
            this.manager = manager;
        }

        public ScreenInfo getScreenInfo() {
            try {
                Object displayInfo = manager.getClass().getMethod("getDisplayInfo", int.class).invoke(manager, 0);
                Class<?> cls = displayInfo.getClass();
                // width and height do not depend on the rotation
                int width = (Integer) cls.getMethod("getNaturalWidth").invoke(displayInfo);
                int height = (Integer) cls.getMethod("getNaturalHeight").invoke(displayInfo);
                int rotation = cls.getDeclaredField("rotation").getInt(displayInfo);
                return new ScreenInfo(width, height, rotation);
            } catch (Exception e) {
                throw new AssertionError(e);
            }
        }
    }
}
