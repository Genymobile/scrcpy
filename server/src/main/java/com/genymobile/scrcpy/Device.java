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
        screenInfo = computeScreenInfo(options.getCrop(), options.getMaxSize());
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

    private ScreenInfo computeScreenInfo(Rect crop, int maxSize) {
        DisplayInfo displayInfo = serviceManager.getDisplayManager().getDisplayInfo();
        boolean rotated = (displayInfo.getRotation() & 1) != 0;
        Size deviceSize = displayInfo.getSize();
        Rect contentRect = new Rect(0, 0, deviceSize.getWidth(), deviceSize.getHeight());
        if (crop != null) {
            if (rotated) {
                // the crop (provided by the user) is expressed in the natural orientation
                crop = flipRect(crop);
            }
            if (!contentRect.intersect(crop)) {
                // intersect() changes contentRect so that it is intersected with crop
                Ln.w("Crop rectangle (" + formatCrop(crop) + ") does not intersect device screen (" + formatCrop(deviceSize.toRect()) + ")");
                contentRect = new Rect(); // empty
            }
        }

        Size videoSize = computeVideoSize(contentRect.width(), contentRect.height(), maxSize);
        return new ScreenInfo(contentRect, videoSize, rotated);
    }

    private static String formatCrop(Rect rect) {
        return rect.width() + ":" + rect.height() + ":" + rect.left + ":" + rect.top;
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static Size computeVideoSize(int w, int h, int maxSize) {
        // Compute the video size and the padding of the content inside this video.
        // Principle:
        // - scale down the great side of the screen to maxSize (if necessary);
        // - scale down the other side so that the aspect ratio is preserved;
        // - round this value to the nearest multiple of 8 (H.264 only accepts multiples of 8)
        w &= ~7; // in case it's not a multiple of 8
        h &= ~7;
        if (maxSize > 0) {
            if (BuildConfig.DEBUG && maxSize % 8 != 0) {
                throw new AssertionError("Max size must be a multiple of 8");
            }
            boolean portrait = h > w;
            int major = portrait ? h : w;
            int minor = portrait ? w : h;
            if (major > maxSize) {
                int minorExact = minor * maxSize / major;
                // +4 to round the value to the nearest multiple of 8
                minor = (minorExact + 4) & ~7;
                major = maxSize;
            }
            w = portrait ? minor : major;
            h = portrait ? major : minor;
        }
        return new Size(w, h);
    }

    public Point getPhysicalPoint(Position position) {
        // it hides the field on purpose, to read it with a lock
        @SuppressWarnings("checkstyle:HiddenField")
        ScreenInfo screenInfo = getScreenInfo(); // read with synchronization
        Size videoSize = screenInfo.getVideoSize();
        Size clientVideoSize = position.getScreenSize();
        if (!videoSize.equals(clientVideoSize)) {
            // The client sends a click relative to a video with wrong dimensions,
            // the device may have been rotated since the event was generated, so ignore the event
            return null;
        }
        Rect contentRect = screenInfo.getContentRect();
        Point point = position.getPoint();
        int scaledX = contentRect.left + point.x * contentRect.width() / videoSize.getWidth();
        int scaledY = contentRect.top + point.y * contentRect.height() / videoSize.getHeight();
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

    static Rect flipRect(Rect crop) {
        return new Rect(crop.top, crop.left, crop.bottom, crop.right);
    }
}
