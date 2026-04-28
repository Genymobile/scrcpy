package com.genymobile.scrcpy.display;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.wrappers.DisplayManager;
import com.genymobile.scrcpy.wrappers.DisplayWindowListener;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.content.res.Configuration;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.IDisplayWindowListener;

public class DisplayMonitor {

    public interface Listener {
        void onDisplayPropertiesChanged();
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

    private DisplayProperties props;

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
                    Ln.v("DisplayMonitor: onDisplayChanged(" + eventDisplayId + ")");
                }

                if (eventDisplayId == displayId) {
                    try {
                        checkDisplayPropertiesChanged();
                    } catch (Throwable e) {
                        Ln.e("DisplayMonitor error", e);
                        throw e;
                    }
                }
            }, handler);
        } else {
            displayWindowListener = new DisplayWindowListener() {
                @Override
                public void onDisplayConfigurationChanged(int eventDisplayId, Configuration newConfig) {
                    if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                        Ln.v("DisplayMonitor: onDisplayConfigurationChanged(" + eventDisplayId + ")");
                    }

                    if (eventDisplayId == displayId) {
                        try {
                            checkDisplayPropertiesChanged();
                        } catch (Throwable e) {
                            Ln.e("DisplayMonitor error", e);
                            throw e;
                        }
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

    private synchronized DisplayProperties getAndSetDisplayProperties(DisplayProperties props) {
        DisplayProperties oldProps = this.props;
        this.props = props;
        return oldProps;
    }

    public synchronized void setSessionDisplayProperties(DisplayProperties props) {
        this.props = props;
    }

    private void checkDisplayPropertiesChanged() {
        DisplayInfo di = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (di == null) {
            Ln.w("DisplayInfo for " + displayId + " cannot be retrieved");
            // We can't compare with the current properties, so reset unconditionally
            DisplayProperties oldProps = getAndSetDisplayProperties(null); // exchange with synchronization
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("DisplayMonitor: requestReset(): " + oldProps + " -> (unknown)");
            }
            listener.onDisplayPropertiesChanged();
        } else {
            DisplayProperties newProps = new DisplayProperties(di.getSize(), di.getRotation());

            DisplayProperties oldProps = getAndSetDisplayProperties(newProps); // exchange with synchronization
            if (!newProps.equals(oldProps)) {
                // Reset only if the properties are different
                if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                    Ln.v("DisplayMonitor: requestReset(): " + oldProps + " -> " + newProps);
                }
                listener.onDisplayPropertiesChanged();
            } else if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v("DisplayMonitor: DisplayProperties not changed (" + newProps + "): do not requestReset()");
            }
        }
    }
}
