package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.os.Build;
import android.os.IBinder;
import android.view.Surface;

import java.util.List;

public class ScreenEncoder extends SurfaceEncoder implements Device.RotationListener, Device.FoldListener {

    private final Device device;

    private IBinder display;

    public ScreenEncoder(Device device, Streamer streamer, int videoBitRate, int maxFps, List<CodecOption> codecOptions,
            String encoderName, boolean downsizeOnError) {
        super(streamer, videoBitRate, maxFps, codecOptions, encoderName, downsizeOnError);
        this.device = device;
    }

    @Override
    public void onFoldChanged(int displayId, boolean folded) {
        resetCapture.set(true);
    }

    @Override
    public void onRotationChanged(int rotation) {
        resetCapture.set(true);
    }

    @Override
    protected void initialize() {
        display = createDisplay();
        device.setRotationListener(this);
        device.setFoldListener(this);
    }

    @Override
    protected Size getSize() {
        return device.getScreenInfo().getVideoSize();
    }

    @Override
    protected void setSize(int size) {
        device.setMaxSize(size);
    }

    @Override
    protected void setSurface(Surface surface) {
        ScreenInfo screenInfo = device.getScreenInfo();
        Rect contentRect = screenInfo.getContentRect();

        // does not include the locked video orientation
        Rect unlockedVideoRect = screenInfo.getUnlockedVideoSize().toRect();
        int videoRotation = screenInfo.getVideoRotation();
        int layerStack = device.getLayerStack();
        setDisplaySurface(display, surface, videoRotation, contentRect, unlockedVideoRect, layerStack);
    }

    @Override
    protected void dispose() {
        device.setRotationListener(null);
        device.setFoldListener(null);
        SurfaceControl.destroyDisplay(display);
    }

    private static IBinder createDisplay() {
        // Since Android 12 (preview), secure displays could not be created with shell permissions anymore.
        // On Android 12 preview, SDK_INT is still R (not S), but CODENAME is "S".
        boolean secure = Build.VERSION.SDK_INT < Build.VERSION_CODES.R || (Build.VERSION.SDK_INT == Build.VERSION_CODES.R && !"S"
                        .equals(Build.VERSION.CODENAME));
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
