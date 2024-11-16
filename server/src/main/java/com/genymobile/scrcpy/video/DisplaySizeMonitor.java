package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.wrappers.DisplayManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.IDisplayFoldListener;
import android.view.IRotationWatcher;

public class DisplaySizeMonitor {

    public interface Listener {
        void onDisplaySizeChanged();
    }

    private DisplayManager.DisplayListenerHandle displayListenerHandle;
    private HandlerThread handlerThread;

    // On Android 14, DisplayListener may be broken (it never sends events). This is fixed in recent Android 14 upgrades, but we can't really
    // detect it directly, so register a RotationWatcher and a DisplayFoldListener as a fallback, until we receive the first event from
    // DisplayListener (which proves that it works).
    private boolean displayListenerWorks; // only accessed from the display listener thread
    private IRotationWatcher rotationWatcher;
    private IDisplayFoldListener displayFoldListener;

    private int displayId = Device.DISPLAY_ID_NONE;

    private Size sessionDisplaySize;

    private Listener listener;

    public void start(int displayId, Listener listener) {
        // Once started, the listener and the displayId must never change
        assert listener != null;
        this.listener = listener;

        assert this.displayId == Device.DISPLAY_ID_NONE;
        this.displayId = displayId;

        handlerThread = new HandlerThread("DisplayListener");
        handlerThread.start();
        Handler handler = new Handler(handlerThread.getLooper());

        if (Build.VERSION.SDK_INT == AndroidVersions.API_34_ANDROID_14) {
            registerDisplayListenerFallbacks();
        }

        displayListenerHandle = ServiceManager.getDisplayManager().registerDisplayListener(eventDisplayId -> {
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("DisplaySizeMonitor: onDisplayChanged(" + eventDisplayId + ")");
            }

            if (Build.VERSION.SDK_INT == AndroidVersions.API_34_ANDROID_14) {
                if (!displayListenerWorks) {
                    // On the first display listener event, we know it works, we can unregister the fallbacks
                    displayListenerWorks = true;
                    unregisterDisplayListenerFallbacks();
                }
            }

            if (eventDisplayId == displayId) {
                checkDisplaySizeChanged();
            }
        }, handler);
    }

    /**
     * Stop and release the monitor.
     * <p/>
     * It must not be used anymore.
     * It is ok to call this method even if {@link #start(int, Listener)} was not called.
     */
    public void stopAndRelease() {
        if (Build.VERSION.SDK_INT == AndroidVersions.API_34_ANDROID_14) {
            unregisterDisplayListenerFallbacks();
        }

        // displayListenerHandle may be null if registration failed
        if (displayListenerHandle != null) {
            ServiceManager.getDisplayManager().unregisterDisplayListener(displayListenerHandle);
            displayListenerHandle = null;
        }

        if (handlerThread != null) {
            handlerThread.quitSafely();
        }
    }

    private synchronized Size getSessionDisplaySize() {
        return sessionDisplaySize;
    }

    public synchronized void setSessionDisplaySize(Size sessionDisplaySize) {
        this.sessionDisplaySize = sessionDisplaySize;
    }

    private void checkDisplaySizeChanged() {
        DisplayInfo di = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (di == null) {
            Ln.w("DisplayInfo for " + displayId + " cannot be retrieved");
            // We can't compare with the current size, so reset unconditionally
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("DisplaySizeMonitor: requestReset(): " + getSessionDisplaySize() + " -> (unknown)");
            }
            setSessionDisplaySize(null);
            listener.onDisplaySizeChanged();
        } else {
            Size size = di.getSize();

            // The field is hidden on purpose, to read it with synchronization
            @SuppressWarnings("checkstyle:HiddenField")
            Size sessionDisplaySize = getSessionDisplaySize(); // synchronized

            // .equals() also works if sessionDisplaySize == null
            if (!size.equals(sessionDisplaySize)) {
                // Reset only if the size is different
                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Ln.v("DisplaySizeMonitor: requestReset(): " + sessionDisplaySize + " -> " + size);
                }
                // Set the new size immediately, so that a future onDisplayChanged() event called before the asynchronous prepare()
                // considers that the current size is the requested size (to avoid a duplicate requestReset())
                setSessionDisplaySize(size);
                listener.onDisplaySizeChanged();
            } else if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("DisplaySizeMonitor: Size not changed (" + size + "): do not requestReset()");
            }
        }
    }

    private void registerDisplayListenerFallbacks() {
        rotationWatcher = new IRotationWatcher.Stub() {
            @Override
            public void onRotationChanged(int rotation) {
                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Ln.v("DisplaySizeMonitor: onRotationChanged(" + rotation + ")");
                }

                checkDisplaySizeChanged();
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
                    Ln.v("DisplaySizeMonitor: onDisplayFoldChanged(" + displayId + ", " + folded + ")");
                }

                if (DisplaySizeMonitor.this.displayId != displayId) {
                    // Ignore events related to other display ids
                    return;
                }

                checkDisplaySizeChanged();
            }
        };
        ServiceManager.getWindowManager().registerDisplayFoldListener(displayFoldListener);
    }

    private synchronized void unregisterDisplayListenerFallbacks() {
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
