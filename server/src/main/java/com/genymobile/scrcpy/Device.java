package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build;
import android.os.RemoteException;
import android.view.IRotationWatcher;
import android.view.InputEvent;

public final class Device {

    public interface RotationListener {
        void onRotationChanged(int rotation);
    }

    private final ServiceManager serviceManager = new ServiceManager();

    private ScreenInfo screenInfo;
    private RotationListener rotationListener;

    public Device(Options options) {
        options.setCrop(new Rect(100, 100, 300, 250));
        screenInfo = computeScreenInfo(options.getMaxSize(), options.getCrop());
        registerRotationWatcher(new IRotationWatcher.Stub() {
            @Override
            public void onRotationChanged(int rotation) throws RemoteException {
                synchronized (Device.this) {
                    screenInfo = screenInfo.withRotation(rotation);

                    // notify
                    if (rotationListener != null) {
                        rotationListener.onRotationChanged(rotation);
                    }
                }
            }
        });
    }

    public synchronized ScreenInfo getScreenInfo() {
        return screenInfo;
    }

    private ScreenInfo computeScreenInfo(int maxSize, Rect crop) {
        DisplayInfo displayInfo = serviceManager.getDisplayManager().getDisplayInfo();
        return ScreenInfo.create(displayInfo, maxSize, crop);
    }

    public Point getPhysicalPoint(Position position) {
        @SuppressWarnings("checkstyle:HiddenField") // it hides the field on purpose, to read it with a lock
        ScreenInfo screenInfo = getScreenInfo(); // read with synchronization
        Size videoSize = screenInfo.getVideoSize();
        Size clientVideoSize = position.getScreenSize();
        if (!videoSize.equals(clientVideoSize)) {
            // The client sends a click relative to a video with wrong dimensions,
            // the device may have been rotated since the event was generated, so ignore the event
            return null;
        }
        Size deviceSize = screenInfo.getDeviceSize();
        Point point = position.getPoint();
        int scaledX = point.x * deviceSize.getWidth() / videoSize.getWidth();
        int scaledY = point.y * deviceSize.getHeight() / videoSize.getHeight();
        return new Point(scaledX, scaledY);
    }

    public static String getDeviceName() {
        return Build.MODEL;
    }

    public boolean injectInputEvent(InputEvent inputEvent, int mode) {
        return serviceManager.getInputManager().injectInputEvent(inputEvent, mode);
    }

    public boolean isScreenOn() {
        return serviceManager.getPowerManager().isScreenOn();
    }

    public void registerRotationWatcher(IRotationWatcher rotationWatcher) {
        serviceManager.getWindowManager().registerRotationWatcher(rotationWatcher);
    }

    public synchronized void setRotationListener(RotationListener rotationListener) {
        this.rotationListener = rotationListener;
    }
}
