package com.genymobile.scrcpy;

import android.os.Build;
import android.view.IRotationWatcher;

import com.genymobile.scrcpy.wrappers.ServiceManager;

public class DeviceUtil {

    private static final ServiceManager serviceManager = new ServiceManager();

    public static ScreenInfo getScreenInfo() {
        return serviceManager.getDisplayManager().getScreenInfo();
    }

    public static void registerRotationWatcher(IRotationWatcher rotationWatcher) {
        serviceManager.getWindowManager().registerRotationWatcher(rotationWatcher);
    }

    public static String getDeviceName() {
        return Build.MODEL;
    }
}
