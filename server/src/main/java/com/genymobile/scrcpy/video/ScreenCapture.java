package com.genymobile.scrcpy.video;

import android.content.Context;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.IBinder;
import android.util.DisplayMetrics;
import android.view.Surface;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;

import java.lang.reflect.Constructor;

public class ScreenCapture extends SurfaceCapture implements Device.RotationListener, Device.FoldListener {

    private final Device device;
    private IBinder display;
    private VirtualDisplay virtualDisplay;
    private boolean isFresh = false;
    private int lastWidth;
    private int lastHeight;

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
        // change the surface
        if (isFresh && virtualDisplay != null) {
            Size s = getSize();
            DisplayMetrics m = new DisplayMetrics();
            virtualDisplay.getDisplay().getMetrics(m);
            Ln.d("Real resize: size is " + s.getWidth() + "x" + s.getHeight() + " dpi=" + m.densityDpi);
            if (m.widthPixels == s.getWidth() && m.heightPixels + 24 == s.getHeight()) {
                Ln.d("Virtual display is same size, nothing to do");
            } else {
                Ln.d("Old size is " + m.widthPixels + "x" + m.heightPixels);
                virtualDisplay.resize(s.getWidth(), s.getHeight(), m.densityDpi);
            }
            virtualDisplay.setSurface(surface);
            int i = virtualDisplay.getDisplay().getDisplayId();
            device.setDisplayId(i);
            return;
        }

        ScreenInfo screenInfo = device.getScreenInfo();
        Rect contentRect = screenInfo.getContentRect();

        // does not include the locked video orientation
        Rect unlockedVideoRect = screenInfo.getUnlockedVideoSize().toRect();
        int videoRotation = screenInfo.getVideoRotation();
        int layerStack = device.getLayerStack();

        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }

                Rect videoRect = screenInfo.getVideoSize().toRect();
        if (device.getDisplayId() >= 0) {
            try {
                virtualDisplay = ServiceManager.getDisplayManager()
                        .createVirtualDisplay("scrcpy", videoRect.width(), videoRect.height(), device.getDisplayId(), surface);
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
        } else {
            try {
                Constructor<DisplayManager> ctor = DisplayManager.class.getDeclaredConstructor(Context.class);
                ctor.setAccessible(true);
                DisplayManager dm = ctor.newInstance(FakeContext.get());
                Ln.d("Video rect = " + videoRect.width() + "x" + videoRect.height());
                virtualDisplay = dm
                        .createVirtualDisplay("scrcpy", videoRect.width(), videoRect.height(),
                                /*FIXME: configure DPI*/160, surface,
                                /*flags*/ 1 << 0 | 1 << 3 | 1 << 6 | 1 << 8 | 1 << 9 | 1 << 10 | 1 << 11 | 1 << 12 | 1 << 13 | 1 << 14);
                int i = virtualDisplay.getDisplay().getDisplayId();
                device.setDisplayId(i);
                Ln.d("Virtual display id is " + i);
                isFresh = true;
            } catch (Exception e) {
                Ln.e("Could not create display using DisplayManager", e);
                throw new AssertionError("Could not create display");
            }
        }
    }

    @Override
    public void release() {
        device.setRotationListener(null);
        device.setFoldListener(null);
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
        return device.getScreenInfo().getVideoSize();
    }

    @Override
    public boolean setMaxSize(int maxSize) {
        device.setMaxSize(maxSize);
        return true;
    }

    public void resizeDisplay(int width, int height) {
        if (virtualDisplay == null || !isFresh) {
            Ln.d("Tried to resize without fresh virtual display");
            return;
        }
        DisplayMetrics m = new DisplayMetrics();
        virtualDisplay.getDisplay().getMetrics(m);
        if (m.widthPixels == width && m.heightPixels + 24 == height) {
            Ln.d("Virtual display is same size, nothing to do");
            return;
        }
        Ln.d("New size is " + width + "x" + height + " dpi=" + m.densityDpi);
        virtualDisplay.setSurface(null);
        virtualDisplay.resize(width, height, m.densityDpi);
        //resizeWidth = width;
        //resizeHeight = height;
        //isResizing.set(true);

        // makes sure that fallback code works fine
        //device.setDisplayId(-1);
        // Device should set the new screenInfo
        requestReset();
        // hack
        //resetCapture.getAndSet(true);
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
