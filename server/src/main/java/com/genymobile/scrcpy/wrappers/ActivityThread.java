package com.genymobile.scrcpy.wrappers;

import java.lang.reflect.Constructor;

public class ActivityThread {

    private static final Class<?> activityThreadClass;
    private static final Object activityThread;

    static {
        try {
            activityThreadClass = Class.forName("android.app.ActivityThread");
            Constructor<?> activityThreadConstructor = activityThreadClass.getDeclaredConstructor();
            activityThreadConstructor.setAccessible(true);
            activityThread = activityThreadConstructor.newInstance();
        } catch (Exception e) {
            throw new AssertionError(e);
        }
    }

    private ActivityThread() {
        // only static methods
    }

    public static Object getActivityThread() {
        return activityThread;
    }

    public static Class<?> getActivityThreadClass() {
        return activityThreadClass;
    }
}
