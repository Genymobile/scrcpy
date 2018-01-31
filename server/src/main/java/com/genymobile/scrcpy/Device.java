package com.genymobile.scrcpy;

import android.graphics.Point;
import android.os.Build;
import android.os.RemoteException;
import android.view.IRotationWatcher;

import com.genymobile.scrcpy.wrappers.InputManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;

public final class Device {

    public interface RotationListener {
        void onRotationChanged(int rotation);
    }

    private final ServiceManager serviceManager = new ServiceManager();

    private ScreenInfo screenInfo;
    private RotationListener rotationListener;

    public Device(Options options) {
        screenInfo = computeScreenInfo(options.getMaximumSize());
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

    private ScreenInfo computeScreenInfo(int maximumSize) {
        // Compute the video size and the padding of the content inside this video.
        // Principle:
        // - scale down the great side of the screen to maximumSize (if necessary);
        // - scale down the other side so that the aspect ratio is preserved;
        // - round this value to the nearest multiple of 8 (H.264 only accepts multiples of 8)
        DisplayInfo displayInfo = serviceManager.getDisplayManager().getDisplayInfo();
        boolean rotated = (displayInfo.getRotation() & 1) != 0;
        Size deviceSize = displayInfo.getSize();
        int w = deviceSize.getWidth();
        int h = deviceSize.getHeight();
        if (maximumSize > 0) {
            assert maximumSize % 8 == 0;
            boolean portrait = h > w;
            int major = portrait ? h : w;
            int minor = portrait ? w : h;
            if (major > maximumSize) {
                int minorExact = minor * maximumSize / major;
                // +4 to round the value to the nearest multiple of 8
                minor = (minorExact + 4) & ~7;
                major = maximumSize;
            }
            w = portrait ? minor : major;
            h = portrait ? major : minor;
        }
        Size videoSize = new Size(w, h);
        return new ScreenInfo(deviceSize, videoSize, rotated);
    }

    public Point getPhysicalPoint(Position position) {
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
