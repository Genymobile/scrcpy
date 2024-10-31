package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.control.PositionMapper;
import com.genymobile.scrcpy.device.ConfigurationException;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.LogUtils;
import com.genymobile.scrcpy.wrappers.DisplayManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;

import android.graphics.Rect;
import android.hardware.display.VirtualDisplay;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
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

    // Source display size (before resizing/crop) for the current session
    private Size sessionDisplaySize;

    private IBinder display;
    private VirtualDisplay virtualDisplay;

    private DisplayManager.DisplayListenerHandle displayListenerHandle;
    private HandlerThread handlerThread;

    // On Android 14, the DisplayListener may be broken (it never sends events). This is fixed in recent Android 14 upgrades, but we can't really
    // detect it directly, so register a RotationWatcher and a DisplayFoldListener as a fallback, until we receive the first event from
    // DisplayListener (which proves that it works).
    private boolean displayListenerWorks; // only accessed from the display listener thread
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
        if (Build.VERSION.SDK_INT == AndroidVersions.API_34_ANDROID_14) {
            registerDisplayListenerFallbacks();
        }

        handlerThread = new HandlerThread("DisplayListener");
        handlerThread.start();
        Handler handler = new Handler(handlerThread.getLooper());
        displayListenerHandle = ServiceManager.getDisplayManager().registerDisplayListener(displayId -> {
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("ScreenCapture: onDisplayChanged(" + displayId + ")");
            }
            if (Build.VERSION.SDK_INT == AndroidVersions.API_34_ANDROID_14) {
                if (!displayListenerWorks) {
                    // On the first display listener event, we know it works, we can unregister the fallbacks
                    displayListenerWorks = true;
                    unregisterDisplayListenerFallbacks();
                }
            }
            if (this.displayId == displayId) {
                DisplayInfo di = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
                if (di == null) {
                    Ln.w("DisplayInfo for " + displayId + " cannot be retrieved");
                    // We can't compare with the current size, so reset unconditionally
                    if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                        Ln.v("ScreenCapture: requestReset(): " + getSessionDisplaySize() + " -> (unknown)");
                    }
                    setSessionDisplaySize(null);
                    invalidate();
                } else {
                    Size size = di.getSize();

                    // The field is hidden on purpose, to read it with synchronization
                    @SuppressWarnings("checkstyle:HiddenField")
                    Size sessionDisplaySize = getSessionDisplaySize(); // synchronized

                    // .equals() also works if sessionDisplaySize == null
                    if (!size.equals(sessionDisplaySize)) {
                        // Reset only if the size is different
                        if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                            Ln.v("ScreenCapture: requestReset(): " + sessionDisplaySize + " -> " + size);
                        }
                        // Set the new size immediately, so that a future onDisplayChanged() event called before the asynchronous prepare()
                        // considers that the current size is the requested size (to avoid a duplicate requestReset())
                        setSessionDisplaySize(size);
                        invalidate();
                    } else if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                        Ln.v("ScreenCapture: Size not changed (" + size + "): do not requestReset()");
                    }
                }
            }
        }, handler);
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

        setSessionDisplaySize(displayInfo.getSize());
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

        int virtualDisplayId;
        PositionMapper positionMapper;
        try {
            Size videoSize = screenInfo.getVideoSize();
            virtualDisplay = ServiceManager.getDisplayManager()
                    .createVirtualDisplay("scrcpy", videoSize.getWidth(), videoSize.getHeight(), displayId, surface);
            virtualDisplayId = virtualDisplay.getDisplay().getDisplayId();
            Rect contentRect = new Rect(0, 0, videoSize.getWidth(), videoSize.getHeight());
            // The position are relative to the virtual display, not the original display
            positionMapper = new PositionMapper(videoSize, contentRect, 0);
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
        if (Build.VERSION.SDK_INT == AndroidVersions.API_34_ANDROID_14) {
            unregisterDisplayListenerFallbacks();
        }

        handlerThread.quitSafely();
        handlerThread = null;

        // displayListenerHandle may be null if registration failed
        if (displayListenerHandle != null) {
            ServiceManager.getDisplayManager().unregisterDisplayListener(displayListenerHandle);
            displayListenerHandle = null;
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
        boolean secure = Build.VERSION.SDK_INT < AndroidVersions.API_30_ANDROID_11 || (Build.VERSION.SDK_INT == AndroidVersions.API_30_ANDROID_11
                && !"S".equals(Build.VERSION.CODENAME));
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

    private synchronized Size getSessionDisplaySize() {
        return sessionDisplaySize;
    }

    private synchronized void setSessionDisplaySize(Size sessionDisplaySize) {
        this.sessionDisplaySize = sessionDisplaySize;
    }

    private void registerDisplayListenerFallbacks() {
        rotationWatcher = new IRotationWatcher.Stub() {
            @Override
            public void onRotationChanged(int rotation) {
                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Ln.v("ScreenCapture: onRotationChanged(" + rotation + ")");
                }
                invalidate();
            }
        };
        ServiceManager.getWindowManager().registerRotationWatcher(rotationWatcher, displayId);

        // Build.VERSION.SDK_INT >= AndroidVersions.API_29_ANDROID_10 (but implied by == API_34_ANDROID 14)
        displayFoldListener = new IDisplayFoldListener.Stub() {

            private boolean first = true;

            @Override
            public void onDisplayFoldChanged(int displayId, boolean folded) {
                if (first) {
                    // An event is posted on registration to signal the initial state. Ignore it to avoid restarting encoding.
                    first = false;
                    return;
                }

                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Ln.v("ScreenCapture: onDisplayFoldChanged(" + displayId + ", " + folded + ")");
                }

                if (ScreenCapture.this.displayId != displayId) {
                    // Ignore events related to other display ids
                    return;
                }
                invalidate();
            }
        };
        ServiceManager.getWindowManager().registerDisplayFoldListener(displayFoldListener);
    }

    private void unregisterDisplayListenerFallbacks() {
        synchronized (this) {
            if (rotationWatcher != null) {
                ServiceManager.getWindowManager().unregisterRotationWatcher(rotationWatcher);
                rotationWatcher = null;
            }
            if (displayFoldListener != null) {
                // Build.VERSION.SDK_INT >= AndroidVersions.API_29_ANDROID_10 (but implied by == API_34_ANDROID 14)
                ServiceManager.getWindowManager().unregisterDisplayFoldListener(displayFoldListener);
                displayFoldListener = null;
            }
        }
    }
}
