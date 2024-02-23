package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ClipboardManager;
import com.genymobile.scrcpy.wrappers.DisplayControl;
import com.genymobile.scrcpy.wrappers.InputManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;
import com.genymobile.scrcpy.wrappers.WindowManager;

import android.content.IOnPrimaryClipChangedListener;
import android.graphics.Rect;
import android.os.Build;
import android.os.IBinder;
import android.os.SystemClock;
import android.view.IDisplayFoldListener;
import android.view.IRotationWatcher;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import java.util.concurrent.atomic.AtomicBoolean;

public final class Device {

    public static final int POWER_MODE_OFF = SurfaceControl.POWER_MODE_OFF;
    public static final int POWER_MODE_NORMAL = SurfaceControl.POWER_MODE_NORMAL;

    public static final int INJECT_MODE_ASYNC = InputManager.INJECT_INPUT_EVENT_MODE_ASYNC;
    public static final int INJECT_MODE_WAIT_FOR_RESULT = InputManager.INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT;
    public static final int INJECT_MODE_WAIT_FOR_FINISH = InputManager.INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH;

    public static final int LOCK_VIDEO_ORIENTATION_UNLOCKED = -1;
    public static final int LOCK_VIDEO_ORIENTATION_INITIAL = -2;

    public interface RotationListener {
        void onRotationChanged(int rotation);
    }

    public interface FoldListener {
        void onFoldChanged(int displayId, boolean folded);
    }

    public interface ClipboardListener {
        void onClipboardTextChanged(String text);
    }

    private final Rect crop;
    private int maxSize;
    private final int lockVideoOrientation;

    private Size deviceSize;
    private ScreenInfo screenInfo;
    private RotationListener rotationListener;
    private FoldListener foldListener;
    private ClipboardListener clipboardListener;
    private final AtomicBoolean isSettingClipboard = new AtomicBoolean();

    /**
     * Logical display identifier
     */
    private final int displayId;

    /**
     * The surface flinger layer stack associated with this logical display
     */
    private final int layerStack;

    private final boolean supportsInputEvents;

    public Device(Options options) throws ConfigurationException {
        displayId = options.getDisplayId();
        DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        if (displayInfo == null) {
            Ln.e("Display " + displayId + " not found\n" + LogUtils.buildDisplayListMessage());
            throw new ConfigurationException("Unknown display id: " + displayId);
        }

        int displayInfoFlags = displayInfo.getFlags();

        deviceSize = displayInfo.getSize();
        crop = options.getCrop();
        maxSize = options.getMaxSize();
        lockVideoOrientation = options.getLockVideoOrientation();

        screenInfo = ScreenInfo.computeScreenInfo(displayInfo.getRotation(), deviceSize, crop, maxSize, lockVideoOrientation);
        layerStack = displayInfo.getLayerStack();

        ServiceManager.getWindowManager().registerRotationWatcher(new IRotationWatcher.Stub() {
            @Override
            public void onRotationChanged(int rotation) {
                synchronized (Device.this) {
                    screenInfo = screenInfo.withDeviceRotation(rotation);

                    // notify
                    if (rotationListener != null) {
                        rotationListener.onRotationChanged(rotation);
                    }
                }
            }
        }, displayId);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ServiceManager.getWindowManager().registerDisplayFoldListener(new IDisplayFoldListener.Stub() {
                @Override
                public void onDisplayFoldChanged(int displayId, boolean folded) {
                    if (Device.this.displayId != displayId) {
                        // Ignore events related to other display ids
                        return;
                    }

                    synchronized (Device.this) {
                        DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
                        if (displayInfo == null) {
                            Ln.e("Display " + displayId + " not found\n" + LogUtils.buildDisplayListMessage());
                            return;
                        }

                        deviceSize = displayInfo.getSize();
                        screenInfo = ScreenInfo.computeScreenInfo(displayInfo.getRotation(), deviceSize, crop, maxSize, lockVideoOrientation);
                        // notify
                        if (foldListener != null) {
                            foldListener.onFoldChanged(displayId, folded);
                        }
                    }
                }
            });
        }

        if (options.getControl() && options.getClipboardAutosync()) {
            // If control and autosync are enabled, synchronize Android clipboard to the computer automatically
            ClipboardManager clipboardManager = ServiceManager.getClipboardManager();
            if (clipboardManager != null) {
                clipboardManager.addPrimaryClipChangedListener(new IOnPrimaryClipChangedListener.Stub() {
                    @Override
                    public void dispatchPrimaryClipChanged() {
                        if (isSettingClipboard.get()) {
                            // This is a notification for the change we are currently applying, ignore it
                            return;
                        }
                        synchronized (Device.this) {
                            if (clipboardListener != null) {
                                String text = getClipboardText();
                                if (text != null) {
                                    clipboardListener.onClipboardTextChanged(text);
                                }
                            }
                        }
                    }
                });
            } else {
                Ln.w("No clipboard manager, copy-paste between device and computer will not work");
            }
        }

        if ((displayInfoFlags & DisplayInfo.FLAG_SUPPORTS_PROTECTED_BUFFERS) == 0) {
            Ln.w("Display doesn't have FLAG_SUPPORTS_PROTECTED_BUFFERS flag, mirroring can be restricted");
        }

        // main display or any display on Android >= Q
        supportsInputEvents = displayId == 0 || Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
        if (!supportsInputEvents) {
            Ln.w("Input events are not supported for secondary displays before Android 10");
        }
    }

    public int getDisplayId() {
        return displayId;
    }

    public synchronized void setMaxSize(int newMaxSize) {
        maxSize = newMaxSize;
        screenInfo = ScreenInfo.computeScreenInfo(screenInfo.getReverseVideoRotation(), deviceSize, crop, newMaxSize, lockVideoOrientation);
    }

    public synchronized ScreenInfo getScreenInfo() {
        return screenInfo;
    }

    public int getLayerStack() {
        return layerStack;
    }

    public Point getPhysicalPoint(Position position) {
        // it hides the field on purpose, to read it with a lock
        @SuppressWarnings("checkstyle:HiddenField")
        ScreenInfo screenInfo = getScreenInfo(); // read with synchronization

        // ignore the locked video orientation, the events will apply in coordinates considered in the physical device orientation
        Size unlockedVideoSize = screenInfo.getUnlockedVideoSize();

        int reverseVideoRotation = screenInfo.getReverseVideoRotation();
        // reverse the video rotation to apply the events
        Position devicePosition = position.rotate(reverseVideoRotation);

        Size clientVideoSize = devicePosition.getScreenSize();
        if (!unlockedVideoSize.equals(clientVideoSize)) {
            // The client sends a click relative to a video with wrong dimensions,
            // the device may have been rotated since the event was generated, so ignore the event
            return null;
        }
        Rect contentRect = screenInfo.getContentRect();
        Point point = devicePosition.getPoint();
        int convertedX = contentRect.left + point.getX() * contentRect.width() / unlockedVideoSize.getWidth();
        int convertedY = contentRect.top + point.getY() * contentRect.height() / unlockedVideoSize.getHeight();
        return new Point(convertedX, convertedY);
    }

    public static String getDeviceName() {
        return Build.MODEL;
    }

    public static boolean supportsInputEvents(int displayId) {
        return displayId == 0 || Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    }

    public boolean supportsInputEvents() {
        return supportsInputEvents;
    }

    public static boolean injectEvent(InputEvent inputEvent, int displayId, int injectMode) {
        if (!supportsInputEvents(displayId)) {
            throw new AssertionError("Could not inject input event if !supportsInputEvents()");
        }

        if (displayId != 0 && !InputManager.setDisplayId(inputEvent, displayId)) {
            return false;
        }

        return ServiceManager.getInputManager().injectInputEvent(inputEvent, injectMode);
    }

    public boolean injectEvent(InputEvent event, int injectMode) {
        return injectEvent(event, displayId, injectMode);
    }

    public static boolean injectKeyEvent(int action, int keyCode, int repeat, int metaState, int displayId, int injectMode) {
        long now = SystemClock.uptimeMillis();
        KeyEvent event = new KeyEvent(now, now, action, keyCode, repeat, metaState, KeyCharacterMap.VIRTUAL_KEYBOARD, 0, 0,
                InputDevice.SOURCE_KEYBOARD);
        return injectEvent(event, displayId, injectMode);
    }

    public boolean injectKeyEvent(int action, int keyCode, int repeat, int metaState, int injectMode) {
        return injectKeyEvent(action, keyCode, repeat, metaState, displayId, injectMode);
    }

    public static boolean pressReleaseKeycode(int keyCode, int displayId, int injectMode) {
        return injectKeyEvent(KeyEvent.ACTION_DOWN, keyCode, 0, 0, displayId, injectMode)
                && injectKeyEvent(KeyEvent.ACTION_UP, keyCode, 0, 0, displayId, injectMode);
    }

    public boolean pressReleaseKeycode(int keyCode, int injectMode) {
        return pressReleaseKeycode(keyCode, displayId, injectMode);
    }

    public static boolean isScreenOn() {
        return ServiceManager.getPowerManager().isScreenOn();
    }

    public synchronized void setRotationListener(RotationListener rotationListener) {
        this.rotationListener = rotationListener;
    }

    public synchronized void setFoldListener(FoldListener foldlistener) {
        this.foldListener = foldlistener;
    }

    public synchronized void setClipboardListener(ClipboardListener clipboardListener) {
        this.clipboardListener = clipboardListener;
    }

    public static void expandNotificationPanel() {
        ServiceManager.getStatusBarManager().expandNotificationsPanel();
    }

    public static void expandSettingsPanel() {
        ServiceManager.getStatusBarManager().expandSettingsPanel();
    }

    public static void collapsePanels() {
        ServiceManager.getStatusBarManager().collapsePanels();
    }

    public static String getClipboardText() {
        ClipboardManager clipboardManager = ServiceManager.getClipboardManager();
        if (clipboardManager == null) {
            return null;
        }
        CharSequence s = clipboardManager.getText();
        if (s == null) {
            return null;
        }
        return s.toString();
    }

    public boolean setClipboardText(String text) {
        ClipboardManager clipboardManager = ServiceManager.getClipboardManager();
        if (clipboardManager == null) {
            return false;
        }

        String currentClipboard = getClipboardText();
        if (currentClipboard != null && currentClipboard.equals(text)) {
            // The clipboard already contains the requested text.
            // Since pasting text from the computer involves setting the device clipboard, it could be set twice on a copy-paste. This would cause
            // the clipboard listeners to be notified twice, and that would flood the Android keyboard clipboard history. To workaround this
            // problem, do not explicitly set the clipboard text if it already contains the expected content.
            return false;
        }

        isSettingClipboard.set(true);
        boolean ok = clipboardManager.setText(text);
        isSettingClipboard.set(false);
        return ok;
    }

    /**
     * @param mode one of the {@code POWER_MODE_*} constants
     */
    public static boolean setScreenPowerMode(int mode) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // On Android 14, these internal methods have been moved to DisplayControl
            boolean useDisplayControl =
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE && !SurfaceControl.hasPhysicalDisplayIdsMethod();

            // Change the power mode for all physical displays
            long[] physicalDisplayIds = useDisplayControl ? DisplayControl.getPhysicalDisplayIds() : SurfaceControl.getPhysicalDisplayIds();
            if (physicalDisplayIds == null) {
                Ln.e("Could not get physical display ids");
                return false;
            }

            boolean allOk = true;
            for (long physicalDisplayId : physicalDisplayIds) {
                IBinder binder = useDisplayControl ? DisplayControl.getPhysicalDisplayToken(
                        physicalDisplayId) : SurfaceControl.getPhysicalDisplayToken(physicalDisplayId);
                allOk &= SurfaceControl.setDisplayPowerMode(binder, mode);
            }
            return allOk;
        }

        // Older Android versions, only 1 display
        IBinder d = SurfaceControl.getBuiltInDisplay();
        if (d == null) {
            Ln.e("Could not get built-in display");
            return false;
        }
        return SurfaceControl.setDisplayPowerMode(d, mode);
    }

    public static boolean powerOffScreen(int displayId) {
        if (!isScreenOn()) {
            return true;
        }
        return pressReleaseKeycode(KeyEvent.KEYCODE_POWER, displayId, Device.INJECT_MODE_ASYNC);
    }

    /**
     * Disable auto-rotation (if enabled), set the screen rotation and re-enable auto-rotation (if it was enabled).
     */
    public void rotateDevice() {
        WindowManager wm = ServiceManager.getWindowManager();

        boolean accelerometerRotation = !wm.isRotationFrozen(displayId);

        int currentRotation = getCurrentRotation(displayId);
        int newRotation = (currentRotation & 1) ^ 1; // 0->1, 1->0, 2->1, 3->0
        String newRotationString = newRotation == 0 ? "portrait" : "landscape";

        Ln.i("Device rotation requested: " + newRotationString);
        wm.freezeRotation(displayId, newRotation);

        // restore auto-rotate if necessary
        if (accelerometerRotation) {
            wm.thawRotation(displayId);
        }
    }

    private static int getCurrentRotation(int displayId) {
        if (displayId == 0) {
            return ServiceManager.getWindowManager().getRotation();
        }

        DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        return displayInfo.getRotation();
    }
}
