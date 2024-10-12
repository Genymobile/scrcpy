package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.control.PositionMapper;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.IBinder;
import android.view.IDisplayFoldListener;
import android.view.IRotationWatcher;
import android.view.Surface;

public class ScreenCapture extends SurfaceCapture {

    private final VirtualDisplayListener vdListener;
    private final int displayId;
    private int maxSize;
    private final Rect crop;
    private final int lockVideoOrientation;

    private DisplayInfo displayInfo;
    private ScreenInfo screenInfo;

    private IBinder display;
    private VirtualDisplay virtualDisplay;

    private IRotationWatcher rotationWatcher;
    private IDisplayFoldListener displayFoldListener;

    public ScreenCapture(VirtualDisplayListener vdListener, int displayId, int maxSize, Rect crop, int lockVideoOrientation) {
        this.vdListener = vdListener;
        this.displayId = displayId;
        this.maxSize = maxSize;
        this.crop = crop;
        this.lockVideoOrientation = lockVideoOrientation;
    }

    @Override
    public void init() {
        if (displayId == 0) {
            rotationWatcher = new IRotationWatcher.Stub() {
                @Override
                public void onRotationChanged(int rotation) {
                    requestReset();
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

                    requestReset();
                }
            };
            ServiceManager.getWindowManager().registerDisplayFoldListener(displayFoldListener);
        }
    }

    @Override
    public void prepare() {
        displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (displayInfo == null) {
            Ln.e("Display " + displayId + " not found\n" + LogUtils.buildDisplayListMessage());
            throw new AssertionError("Display " + display + " not found");
        }

        if ((displayInfo.getFlags() & DisplayInfo.FLAG_SUPPORTS_PROTECTED_BUFFERS) == 0) {
            Ln.w("Display doesn't have FLAG_SUPPORTS_PROTECTED_BUFFERS flag, mirroring can be restricted");
        }

        screenInfo = ScreenInfo.computeScreenInfo(displayInfo.getRotation(), displayInfo.getSize(), crop, maxSize, lockVideoOrientation);
    }

    @Override
    public void start(Surface surface) {
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }

        Size displaySize = screenInfo.getVideoSize();

        int virtualDisplayId;
        PositionMapper positionMapper;
        try {
            virtualDisplay = ServiceManager.getDisplayManager()
                    .createVirtualDisplay("scrcpy", displaySize.getWidth(), displaySize.getHeight(), displayId, surface);
            virtualDisplayId = virtualDisplay.getDisplay().getDisplayId();
            Rect contentRect = new Rect(0, 0, displaySize.getWidth(), displaySize.getHeight());
            // The position are relative to the virtual display, not the original display
            positionMapper = new PositionMapper(displaySize, contentRect, 0);
            Ln.d("Display: using DisplayManager API");
        } catch (Exception displayManagerException) {
            try {
                display = createDisplay();

                Rect contentRect = screenInfo.getContentRect();

                // does not include the locked video orientation
                Rect unlockedVideoRect = screenInfo.getUnlockedVideoSize().toRect();
                int videoRotation = screenInfo.getVideoRotation();
                int layerStack = displayInfo.getLayerStack();

                setDisplaySurface(display, surface, videoRotation, contentRect, unlockedVideoRect, layerStack);
                virtualDisplayId = displayId;
                positionMapper = PositionMapper.from(screenInfo);
                Ln.d("Display: using SurfaceControl API");
            } catch (Exception surfaceControlException) {
                Ln.e("Could not create display using DisplayManager", displayManagerException);
                Ln.e("Could not create display using SurfaceControl", surfaceControlException);
                throw new AssertionError("Could not create display");
            }
        }

        if (vdListener != null) {
            vdListener.onNewVirtualDisplay(virtualDisplayId, positionMapper);
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
    public Size getSize() {
        return screenInfo.getVideoSize();
    }

    @Override
    public boolean setMaxSize(int newMaxSize) {
        maxSize = newMaxSize;
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
