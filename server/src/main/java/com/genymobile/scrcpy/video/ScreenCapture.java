package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.control.PositionMapper;
import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Orientation;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.opengl.AffineOpenGLFilter;
import com.genymobile.scrcpy.opengl.OpenGLFilter;
import com.genymobile.scrcpy.opengl.OpenGLRunner;
import com.genymobile.scrcpy.util.AffineMatrix;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.content.res.Resources;
import android.graphics.Rect;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.IBinder;
import android.view.Surface;
import android.hardware.display.DisplayManager;

import java.io.IOException;

public class ScreenCapture extends SurfaceCapture {

    private final VirtualDisplayListener vdListener;
    private final int displayId;
    private int maxSize;
    private final Rect crop;
    private Orientation.Lock captureOrientationLock;
    private Orientation captureOrientation;
    private final float angle;

    private DisplayInfo displayInfo;
    private Size videoSize;

    private final DisplaySizeMonitor displaySizeMonitor = new DisplaySizeMonitor();

    private IBinder display;
    private VirtualDisplay virtualDisplay;

    private AffineMatrix transform;
    private OpenGLRunner glRunner;

    public ScreenCapture(VirtualDisplayListener vdListener, Options options) {
        this.vdListener = vdListener;
        this.displayId = options.getDisplayId();
        assert displayId != Device.DISPLAY_ID_NONE;
        this.maxSize = options.getMaxSize();
        this.crop = options.getCrop();
        this.captureOrientationLock = options.getCaptureOrientationLock();
        this.captureOrientation = options.getCaptureOrientation();
        assert captureOrientationLock != null;
        assert captureOrientation != null;
        this.angle = options.getAngle();
    }

    @Override
    public void init() {
        displaySizeMonitor.start(displayId, this::invalidate);
    }

    @Override
    public void prepare() throws ConfigurationException {
        displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (displayInfo == null) {
            Ln.e("Display " + displayId + " not found\n" + LogUtils.buildDisplayListMessage());
            throw new ConfigurationException("Unknown display id: " + displayId);
        }

        if ((displayInfo.getFlags() & DisplayInfo.FLAG_SUPPORTS_PROTECTED_BUFFERS) == 0) {
            Ln.w("Display doesn't have FLAG_SUPPORTS_PROTECTED_BUFFERS flag, mirroring can be restricted");
        }

        Size displaySize = displayInfo.getSize();
        displaySizeMonitor.setSessionDisplaySize(displaySize);



        VideoFilter filter = new VideoFilter(displaySize);

        if (crop != null) {
            boolean transposed = (displayInfo.getRotation() % 2) != 0;
            filter.addCrop(crop, transposed);
        }

        if (captureOrientationLock == Orientation.Lock.LockedInitial) {
            // The user requested to lock the video orientation to the current orientation
            captureOrientationLock = Orientation.Lock.LockedValue;
            captureOrientation = Orientation.fromRotation(displayInfo.getRotation());
        }

        boolean locked = captureOrientationLock != Orientation.Lock.Unlocked;
        filter.addOrientation(displayInfo.getRotation(), locked, captureOrientation);
        filter.addAngle(angle);

        transform = filter.getInverseTransform();
        videoSize = filter.getOutputSize().limit(maxSize).round8();
    }

    @Override
    public void start(Surface surface) throws IOException {
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            Ln.i("DESTROY_DISPLAY: " +display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            Ln.i("DESTROY_VIRTUAL_DISPLAY: " +virtualDisplay);
            virtualDisplay = null;
        }

        Size inputSize;
        if (transform != null) {
            // If there is a filter, it must receive the full display content
            inputSize = videoSize;
            assert glRunner == null;
            OpenGLFilter glFilter = new AffineOpenGLFilter(transform);
            glRunner = new OpenGLRunner(glFilter);
            surface = glRunner.start(inputSize, videoSize, surface);
        } else {
            // If there is no filter, the display must be rendered at target video size directly
            inputSize = videoSize;
        }

        try {
            int densityDpi = displayInfo.getDpi();
            int flags    = DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY;
            virtualDisplay = ServiceManager.getDisplayManager()
                    .createVirtualDisplay("scrcpy", inputSize.getWidth(), inputSize.getHeight(), displayId, surface);
            Ln.d("Display: using DisplayManager API");
        } catch (Exception displayManagerException) {
            try {
                display = createDisplay();

                Size deviceSize = displayInfo.getSize();
                int layerStack = displayInfo.getLayerStack();


                waitForDisplayReady(deviceSize, 500);
                waitForDisplayStableResolution("scrcpy", 500, 5);

                setDisplaySurface(display, surface, deviceSize.toRect(), inputSize.toRect(), layerStack);

                Ln.d("Display: using SurfaceControl API");
            } catch (Exception surfaceControlException) {
                Ln.e("Could not create display using DisplayManager", displayManagerException);
                Ln.e("Could not create display using SurfaceControl", surfaceControlException);
                throw new AssertionError("Could not create display");
            }
        }

        if (vdListener != null) {
            int virtualDisplayId;
            PositionMapper positionMapper;
            if (virtualDisplay == null || displayId == 0) {
                // Surface control or main display: send all events to the original display, relative to the device size
                Size deviceSize = displayInfo.getSize();
                positionMapper = PositionMapper.create(videoSize, transform, deviceSize);
                virtualDisplayId = displayId;
            } else {
                // The positions are relative to the virtual display, not the original display (so use inputSize, not deviceSize!)
                positionMapper = PositionMapper.create(videoSize, transform, inputSize);
                virtualDisplayId = virtualDisplay.getDisplay().getDisplayId();
            }
            vdListener.onNewVirtualDisplay(virtualDisplayId, positionMapper);
        }
    }

    @Override
    public void stop() {
        if (glRunner != null) {
            glRunner.stopAndRelease();
            glRunner = null;
        }
    }

    @Override
    public void release() {
        displaySizeMonitor.stopAndRelease();

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
        return videoSize;
    }

    @Override
    public boolean setMaxSize(int newMaxSize) {
        maxSize = newMaxSize;
        return true;
    }

    private static IBinder createDisplay() throws Exception {
        // Since Android 12 (preview), secure displays could not be created with shell permissions anymore.
        // On Android 12 preview, SDK_INT is still R (not S), but CODENAME is "S".
        boolean secure = false;
        return SurfaceControl.createDisplay("scrcpy", secure);
    }

    public void DestroyDisplay(){
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }
    }


    public void setupDisplay(Surface surface){
        if (display != null) {
            SurfaceControl.destroyDisplay(display);
            display = null;
        }

        try{
            display = createDisplay();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }


        Rect deviceRect = displayInfo.getSize().toRect();
        Rect cropRect   = (crop != null) ? crop : deviceRect;
        int layerStack  = displayInfo.getLayerStack();

        SurfaceControl.openTransaction();
        try {
            SurfaceControl.setDisplaySurface(display, surface);
            SurfaceControl.setDisplayProjection(display, 0, deviceRect, cropRect);
            SurfaceControl.setDisplayLayerStack(display, layerStack);
        } finally {
            SurfaceControl.closeTransaction();
        }
    }

    private void waitForDisplayStableResolution(String displayName, long delayMs, int maxTries) {
        int lastWidth = -1;
        int lastHeight = -1;

        for (int i = 0; i < maxTries; i++) {
            DisplayInfo info = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
            if (info != null) {
                int w = info.getSize().getWidth();
                int h = info.getSize().getWidth();
                if (w == lastWidth && h == lastHeight && w > 1 && h > 1) {
                    Ln.d("Display stabilized: " + w + "x" + h);
                    return;
                }
                lastWidth = w;
                lastHeight = h;
            }

            try {
                Thread.sleep(delayMs);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                break;
            }
        }

        Ln.w("Display resolution did not stabilize within expected time");
    }


    private void setDisplaySurface(IBinder display, Surface surface, Rect deviceRect, Rect displayRect, int layerStack) {
        SurfaceControl.openTransaction();
        try {
            SurfaceControl.setDisplaySurface(display, surface);
//            SurfaceControl.setDisplayProjection(display, 0, deviceRect, displayRect);
            Rect cropRect   = this.crop;  // options.getCrop()
            SurfaceControl.setDisplayProjection(display,0,deviceRect,cropRect);

            SurfaceControl.setDisplayLayerStack(display, layerStack);
        } finally {
            SurfaceControl.closeTransaction();
        }
    }

    private void waitForDisplayReady(Size expectedSize, int timeoutMs) {
        final int interval = 50;
        int waited = 0;
        while (waited < timeoutMs) {
            DisplayInfo info = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
            if (info != null && info.getSize().equals(expectedSize)) {
                Ln.i("Display is now ready: " + info.getSize());
                return;
            }
            try {
                Thread.sleep(interval);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return;
            }
            waited += interval;
        }
        Ln.w("Display did not reach expected size in time: " + expectedSize);
    }

    @Override
    public void requestInvalidate() {
        invalidate();
    }
}
