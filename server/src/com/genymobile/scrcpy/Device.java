package com.genymobile.scrcpy;

import android.os.Build;
import android.os.RemoteException;
import android.view.IRotationWatcher;

import com.genymobile.scrcpy.wrappers.InputManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;

public class Device {

    public interface RotationListener {
        void onRotationChanged(int rotation);
    }

    private static final Device INSTANCE = new Device();
    private final ServiceManager serviceManager = new ServiceManager();

    private ScreenInfo screenInfo;
    private RotationListener rotationListener;

    private Device() {
        screenInfo = readScreenInfo();
        registerRotationWatcher(new IRotationWatcher.Stub() {
            @Override
            public void onRotationChanged(int rotation) throws RemoteException {
                synchronized (Device.this) {
                    // update screenInfo cache
                    screenInfo = screenInfo.withRotation(rotation);

                    // notify
                    if (rotationListener != null) {
                        rotationListener.onRotationChanged(rotation);
                    }
                }
            }
        });
    }

    public static Device getInstance() {
        return INSTANCE;
    }

    public synchronized ScreenInfo getScreenInfo() {
        if (screenInfo == null) {
            screenInfo = readScreenInfo();
        }
        return screenInfo;
    }

    public RawPoint getPhysicalPoint(Point point) {
        ScreenInfo screenInfo = getScreenInfo();
        int deviceWidth = screenInfo.getLogicalWidth();
        int deviceHeight = screenInfo.getLogicalHeight();
        int scaledX = point.getX() * deviceWidth / point.getScreenWidth();
        int scaledY = point.getY() * deviceHeight / point.getScreenHeight();
        return new RawPoint(scaledX, scaledY);
    }

    private ScreenInfo readScreenInfo() {
        return serviceManager.getDisplayManager().getScreenInfo();
    }

    public static String getDeviceName() {
        return Build.MODEL;
    }

    public InputManager getInputManager() {
        return serviceManager.getInputManager();
    }

    public void registerRotationWatcher(IRotationWatcher rotationWatcher) {
        serviceManager.getWindowManager().registerRotationWatcher(rotationWatcher);
    }

    public synchronized void setRotationListener(RotationListener rotationListener) {
        this.rotationListener = rotationListener;
    }
}
