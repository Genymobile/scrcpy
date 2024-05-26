package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.hardware.display.VirtualDisplayConfig;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.system.ErrnoException;
import android.system.Os;
import android.view.Surface;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;

public class ScreenCapture extends SurfaceCapture implements Device.RotationListener, Device.FoldListener {

    private final Device device;
    private IBinder display;
    private VirtualDisplay virtualDisplay;

    public ScreenCapture(Device device) {
        this.device = device;
    }

    @Override
    public void init() {
        device.setRotationListener(this);
        device.setFoldListener(this);
    }

    @Override
    @SuppressWarnings("deprecation")
    public void start(Surface surface) throws IOException {
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }
        if (Os.getuid() == 0) {
            Handler handler = new Handler(Looper.getMainLooper());
            CountDownLatch latch = new CountDownLatch(1);
            handler.post(() -> {
                try {
                    Os.seteuid(0);
                    createDisplay(surface);
                    Os.seteuid(2000);
                } catch (ErrnoException ignored) {
                }
                latch.countDown();
            });
            try {
                latch.await();
            } catch (InterruptedException e) {
                throw new IOException(e);
            }
        } else {
            createDisplay(surface);
        }
    }

    private void createDisplay(Surface surface) {
        ScreenInfo screenInfo = device.getScreenInfo();
        Rect contentRect = screenInfo.getContentRect();

        // does not include the locked video orientation
        Rect unlockedVideoRect = screenInfo.getUnlockedVideoSize().toRect();
        int videoRotation = screenInfo.getVideoRotation();
        int layerStack = device.getLayerStack();

        try {
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.TIRAMISU) {
                display = createDisplay();
                setDisplaySurface(display, surface, videoRotation, contentRect, unlockedVideoRect, layerStack);
                Ln.d("Display: using SurfaceControl API");
            } else {
                int flags = DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR;
                if (Os.getuid() == 0) {
                    flags |= DisplayManager.VIRTUAL_DISPLAY_FLAG_SECURE;
                }
                VirtualDisplayConfig.Builder builder = new VirtualDisplayConfig.Builder("scrcpy",
                        unlockedVideoRect.width(), unlockedVideoRect.height(), 1 /* densityDpi */)
                        .setFlags(flags)
                        .setSurface(surface);
                //noinspection JavaReflectionMemberAccess
                builder.getClass().getMethod("setDisplayIdToMirror", int.class).invoke(builder, device.getDisplayId());
                virtualDisplay = ServiceManager.getDisplayManager().createVirtualDisplay(FakeContext.get(), builder.build());
                Ln.d("Display: using DisplayManager API");
            }
        } catch (Exception e) {
            Ln.e("Could not create display", e);
            throw new AssertionError("Could not create display");
        }
    }

    @Override
    public void release() {
        device.setRotationListener(null);
        device.setFoldListener(null);
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
        }
        if (virtualDisplay != null) {
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

    private static IBinder createDisplay() throws Exception {
        // Since Android 12, secure displays could not be created with shell permissions anymore.
        // Since Android 14, this method is deprecated.
        IBinder display = SurfaceControl.createDisplay("scrcpy", true);
        if (display == null) {
            display = SurfaceControl.createDisplay("scrcpy", false);
        }
        return display;
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
