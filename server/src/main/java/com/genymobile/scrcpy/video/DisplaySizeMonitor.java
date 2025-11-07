package com.genymobile.scrcpy.video;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.device.DisplayInfo;
import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.wrappers.DisplayManager;
import com.genymobile.scrcpy.wrappers.DisplayWindowListener;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.content.res.Configuration;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.IDisplayWindowListener;

public class DisplaySizeMonitor {

    public interface Listener {
        void onDisplaySizeChanged();
    }

    // On Android 14, DisplayListener may be broken (it never sends events). This is fixed in recent Android 14 upgrades, but we can't really
    // detect it directly, so register a DisplayWindowListener (introduced in Android 11) to listen to configuration changes instead.
    // It has been broken again after an Android 15 upgrade: <https://github.com/Genymobile/scrcpy/issues/5908>
    // So use the default method only before Android 14.
    private static final boolean USE_DEFAULT_METHOD = Build.VERSION.SDK_INT < AndroidVersions.API_34_ANDROID_14;

    private DisplayManager.DisplayListenerHandle displayListenerHandle;
    private HandlerThread handlerThread;

    private IDisplayWindowListener displayWindowListener;

    private int displayId = Device.DISPLAY_ID_NONE;

    private Size sessionDisplaySize;

    private Listener listener;

    public void start(int displayId, Listener listener) {
        // Once started, the listener and the displayId must never change
        assert listener != null;
        this.listener = listener;

        assert this.displayId == Device.DISPLAY_ID_NONE;
        this.displayId = displayId;

        if (USE_DEFAULT_METHOD) {
            handlerThread = new HandlerThread("DisplayListener");
            handlerThread.start();
            Handler handler = new Handler(handlerThread.getLooper());
            displayListenerHandle = ServiceManager.getDisplayManager().registerDisplayListener(eventDisplayId -> {
                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Ln.v("DisplaySizeMonitor: onDisplayChanged(" + eventDisplayId + ")");
                }

                if (eventDisplayId == displayId) {
                    checkDisplaySizeChanged();
                }
            }, handler);
        } else {
            displayWindowListener = new DisplayWindowListener() {
                @Override
                public void onDisplayConfigurationChanged(int eventDisplayId, Configuration newConfig) {
                    if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                        Ln.v("DisplaySizeMonitor: onDisplayConfigurationChanged(" + eventDisplayId + ")");
                    }

                    if (eventDisplayId == displayId) {
                        checkDisplaySizeChanged();
                    }
                }
            };
            ServiceManager.getWindowManager().registerDisplayWindowListener(displayWindowListener);
        }
    }

    /**
     * Stop and release the monitor.
     * <p/>
     * It must not be used anymore.
     * It is ok to call this method even if {@link #start(int, Listener)} was not called.
     */
    public void stopAndRelease() {
        if (USE_DEFAULT_METHOD) {
            // displayListenerHandle may be null if registration failed
            if (displayListenerHandle != null) {
                ServiceManager.getDisplayManager().unregisterDisplayListener(displayListenerHandle);
                displayListenerHandle = null;
            }

            if (handlerThread != null) {
                handlerThread.quitSafely();
            }
        } else if (displayWindowListener != null) {
            ServiceManager.getWindowManager().unregisterDisplayWindowListener(displayWindowListener);
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
}
