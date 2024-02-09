package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.DisplayManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;
import com.genymobile.scrcpy.wrappers.MediaProjectionGlobal;

import android.graphics.Rect;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.IBinder;
import android.view.Surface;

public class ScreenCapture extends SurfaceCapture implements Device.RotationListener, Device.FoldListener {

    private final Device device;
    private IBinder display;
		VirtualDisplay virtualDisplay;

    public ScreenCapture(Device device) {
        this.device = device;
    }

    @Override
    public void init() {
        device.setRotationListener(this);
        device.setFoldListener(this);
    }

    @Override
    public void start(Surface surface) {
        ScreenInfo screenInfo = device.getScreenInfo();
        Rect contentRect = screenInfo.getContentRect();

        // does not include the locked video orientation
        Rect unlockedVideoRect = screenInfo.getUnlockedVideoSize().toRect();
        int videoRotation = screenInfo.getVideoRotation();
        int layerStack = device.getLayerStack();
				Rect videoRect = screenInfo.getVideoSize().toRect();

        if (display != null) {
            SurfaceControl.destroyDisplay(display);
        }
				try {
					virtualDisplay = DisplayManager.createVirtualDisplay(
                "displayManager VirtualDisplay",
                videoRect.width(),
                videoRect.height(),
                0,
                surface);
          Ln.i("Using the DisplayManager system API.");
				} catch (Exception displayManagerException) {
					try {
						virtualDisplay = MediaProjectionGlobal.createVirtualDisplay(
                  "mediaProjection global",
                  videoRect.width(),
                  videoRect.height(),
                  0,
                  surface);
          	Ln.i("Using the MediaProjectionGlobal system API.");
					} catch (NoSuchMethodException | NullPointerException mediaProjectionGlobalException){
        		display = createDisplay();
        		setDisplaySurface(display, surface, videoRotation, contentRect, unlockedVideoRect, layerStack);
					}
				} 
    }

    @Override
    public void release() {
        device.setRotationListener(null);
        device.setFoldListener(null);
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
        }
				if (virtualDisplay != null){
						virtualDisplay.release();
				}
    }

    @Override
    public Size getSize() {
        return device.getScreenInfo().getVideoSize();
    }

    @Override
    public boolean setMaxSize(int maxSize) {
        device.setMaxSize(maxSize);
        return true;
    }

    @Override
    public void onFoldChanged(int displayId, boolean folded) {
        requestReset();
    }

    @Override
    public void onRotationChanged(int rotation) {
        requestReset();
    }

    private static IBinder createDisplay() {
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
