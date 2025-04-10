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

import java.util.Objects;

public class DisplaySizeMonitor {

    private static class SessionInfo {
        private Size size;
        private int rotation;

        public SessionInfo(Size size, int rotation) {
            this.size = size;
            this.rotation = rotation;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o)
                return true;
            if (!(o instanceof SessionInfo))
                return false;
            SessionInfo that = (SessionInfo) o;
            return rotation == that.rotation && Objects.equals(size, that.size);
        }

        @Override
        public String toString() {
            return "{" + "size=" + size + ", rotation=" + rotation + '}';
        }
    }

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

    private SessionInfo sessionInfo;

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

    private synchronized SessionInfo getSessionInfo() {
        return sessionInfo;
    }

    private synchronized void setSessionInfo(SessionInfo sessionInfo) {
        Ln.e("@@@@@@@@@@@@@@@@@@@@@@"+sessionInfo.rotation);
        this.sessionInfo = sessionInfo;
    }

    public void setSessionInfo(Size size, int rotation) {
        setSessionInfo(new SessionInfo(size, rotation));
    }

    private void checkDisplaySizeChanged() {
        DisplayInfo di = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (di == null) {
            Ln.w("DisplayInfo for " + displayId + " cannot be retrieved");
            // We can't compare with the current size, so reset unconditionally
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("DisplaySizeMonitor: requestReset(): " + getSessionInfo() + " -> (unknown)");
            }
            setSessionInfo(null);
            listener.onDisplaySizeChanged();
        } else {
            SessionInfo si = new SessionInfo(di.getSize(), di.getRotation());

            // The field is hidden on purpose, to read it with synchronization
            @SuppressWarnings("checkstyle:HiddenField")
            SessionInfo sessionInfo = getSessionInfo(); // synchronized

            // .equals() also works if sessionDisplaySize == null
            if (!si.equals(sessionInfo)) {
                // Reset only if the size is different
                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Ln.v("DisplaySizeMonitor: requestReset(): " + sessionInfo + " -> " + si);
                }
                // Set the new size immediately, so that a future onDisplayChanged() event called before the asynchronous prepare()
                // considers that the current size is the requested size (to avoid a duplicate requestReset())
                setSessionInfo(si);
                listener.onDisplaySizeChanged();
            } else if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("DisplaySizeMonitor: Size and rotation not changed (" + sessionInfo + "): do not requestReset()");
            }
        }
    }
}
