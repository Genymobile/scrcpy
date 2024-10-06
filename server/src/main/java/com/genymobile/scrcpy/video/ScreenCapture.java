package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.IBinder;
import android.view.IDisplayFoldListener;
import android.view.IRotationWatcher;
import android.view.Surface;

public class ScreenCapture extends SurfaceCapture {

    private final Device device;

    private final int displayId;
    private int maxSize;
    private final Rect crop;
    private final int lockVideoOrientation;
    private int layerStack;
    private int dpi;

    private Size deviceSize;
    private ScreenInfo screenInfo;

    private IBinder display;
    private VirtualDisplay virtualDisplay;

    private IRotationWatcher rotationWatcher;
    private IDisplayFoldListener displayFoldListener;

    public ScreenCapture(Device device, int displayId, int maxSize, Rect crop, int lockVideoOrientation) {
        this.device = device;
        this.displayId = displayId;
        this.maxSize = maxSize;
        this.crop = crop;
        this.lockVideoOrientation = lockVideoOrientation;
    }

    @Override
    public void init() throws ConfigurationException {
        DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (displayInfo == null) {
            Ln.e("Display " + displayId + " not found\n" + LogUtils.buildDisplayListMessage());
            throw new ConfigurationException("Unknown display id: " + displayId);
        }

        deviceSize = displayInfo.getSize();
        screenInfo = ScreenInfo.computeScreenInfo(displayInfo.getRotation(), deviceSize, crop, maxSize, lockVideoOrientation);
        device.setScreenInfo(screenInfo);
        layerStack = displayInfo.getLayerStack();
        dpi = displayInfo.getLogicalDensityDpi();

        if (displayId == 0) {
            rotationWatcher = new IRotationWatcher.Stub() {
                @Override
                public void onRotationChanged(int rotation) {
                    synchronized (ScreenCapture.this) {
                        screenInfo = screenInfo.withDeviceRotation(rotation);
                        device.setScreenInfo(screenInfo);
                    }
                }
            };
            ServiceManager.getWindowManager().registerRotationWatcher(rotationWatcher, displayId);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            displayFoldListener = new IDisplayFoldListener.Stub() {
                @Override
                public void onDisplayFoldChanged(int displayId, boolean folded) {
                    if (ScreenCapture.this.displayId != displayId) {
                        // Ignore events related to other display ids
                        return;
                    }

                    synchronized (ScreenCapture.this) {
                        DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
                        if (displayInfo == null) {
                            Ln.e("Display " + displayId + " not found\n" + LogUtils.buildDisplayListMessage());
                            return;
                        }

                        deviceSize = displayInfo.getSize();
                        screenInfo = ScreenInfo.computeScreenInfo(displayInfo.getRotation(), deviceSize, crop, maxSize, lockVideoOrientation);
                        device.setScreenInfo(screenInfo);
                    }
                }
            };
            ServiceManager.getWindowManager().registerDisplayFoldListener(displayFoldListener);
        }

        if ((displayInfo.getFlags() & DisplayInfo.FLAG_SUPPORTS_PROTECTED_BUFFERS) == 0) {
            Ln.w("Display doesn't have FLAG_SUPPORTS_PROTECTED_BUFFERS flag, mirroring can be restricted");
        }
    }

    @Override
    public void start(Surface surface) {
        Rect contentRect = screenInfo.getContentRect();

        // does not include the locked video orientation
        Rect unlockedVideoRect = screenInfo.getUnlockedVideoSize().toRect();
        int videoRotation = screenInfo.getVideoRotation();

        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }

        try {
            Rect videoRect = screenInfo.getVideoSize().toRect();
            int flags = DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC | DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY | (1
                    << 6) /* DisplayManager.VIRTUAL_DISPLAY_FLAG_SUPPORT_TOUCH */ | 1 << 8 | 1 << 9 | 1 << 10 | 1 << 11 | 1 << 12 | 1 << 13 | 1 << 14;

            virtualDisplay = ServiceManager.getDisplayManager()
                    .createVirtualDisplay("scrcpy", videoRect.width(), videoRect.height(), dpi, surface, flags);
            device.setVirtualDisplayId(virtualDisplay.getDisplay().getDisplayId());
            Ln.d("Display: using DisplayManager API");
        } catch (Exception displayManagerException) {
            try {
                display = createDisplay();
                setDisplaySurface(display, surface, videoRotation, contentRect, unlockedVideoRect, layerStack);
                Ln.d("Display: using SurfaceControl API");
            } catch (Exception surfaceControlException) {
                Ln.e("Could not create display using DisplayManager", displayManagerException);
                Ln.e("Could not create display using SurfaceControl", surfaceControlException);
                throw new AssertionError("Could not create display");
            }
        }
    }

    @Override
    public void release() {
        if (rotationWatcher != null) {
            ServiceManager.getWindowManager().unregisterRotationWatcher(rotationWatcher);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ServiceManager.getWindowManager().unregisterDisplayFoldListener(displayFoldListener);
        }
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }
    }

    @Override
    public synchronized Size getSize() {
        return screenInfo.getVideoSize();
    }

    @Override
    public synchronized boolean setMaxSize(int newMaxSize) {
        maxSize = newMaxSize;
        screenInfo = ScreenInfo.computeScreenInfo(screenInfo.getReverseVideoRotation(), deviceSize, crop, newMaxSize, lockVideoOrientation);
        device.setScreenInfo(screenInfo);
        return true;
    }

    private static IBinder createDisplay() throws Exception {
        // Since Android 12 (preview), secure displays could not be created with shell permissions anymore.
        // On Android 12 preview, SDK_INT is still R (not S), but CODENAME is "S".
        boolean secure = Build.VERSION.SDK_INT < Build.VERSION_CODES.R || (Build.VERSION.SDK_INT == Build.VERSION_CODES.R && !"S".equals(
                Build.VERSION.CODENAME));
        return SurfaceControl.createDisplay("scrcpy", secure);
    }

    private static void setDisplaySurface(IBinder display, Surface surface, int orientation, Rect deviceRect, Rect displayRect, int layerStack) {
        SurfaceControl.openTransaction();
        try {
            SurfaceControl.setDisplaySurface(display, surface);
            SurfaceControl.setDisplayProjection(display, orientation, deviceRect, displayRect);
            SurfaceControl.setDisplayLayerStack(display, layerStack);
        } finally {
            SurfaceControl.closeTransaction();
        }
    }
}
