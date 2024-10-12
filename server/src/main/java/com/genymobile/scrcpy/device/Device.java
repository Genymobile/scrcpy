package com.genymobile.scrcpy.device;

import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.wrappers.ClipboardManager;
import com.genymobile.scrcpy.wrappers.DisplayControl;
import com.genymobile.scrcpy.wrappers.InputManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;
import com.genymobile.scrcpy.wrappers.WindowManager;

import android.os.Build;
import android.os.IBinder;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

public final class Device {

    public static final int DISPLAY_ID_NONE = -1;

    public static final int POWER_MODE_OFF = SurfaceControl.POWER_MODE_OFF;
    public static final int POWER_MODE_NORMAL = SurfaceControl.POWER_MODE_NORMAL;

    public static final int INJECT_MODE_ASYNC = InputManager.INJECT_INPUT_EVENT_MODE_ASYNC;
    public static final int INJECT_MODE_WAIT_FOR_RESULT = InputManager.INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT;
    public static final int INJECT_MODE_WAIT_FOR_FINISH = InputManager.INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH;

    public static final int LOCK_VIDEO_ORIENTATION_UNLOCKED = -1;
    public static final int LOCK_VIDEO_ORIENTATION_INITIAL = -2;

    private Device() {
        // not instantiable
    }

    public static String getDeviceName() {
        return Build.MODEL;
    }

    public static boolean supportsInputEvents(int displayId) {
        return displayId == 0 || Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
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

    public static boolean injectKeyEvent(int action, int keyCode, int repeat, int metaState, int displayId, int injectMode) {
        long now = SystemClock.uptimeMillis();
        KeyEvent event = new KeyEvent(now, now, action, keyCode, repeat, metaState, KeyCharacterMap.VIRTUAL_KEYBOARD, 0, 0,
                InputDevice.SOURCE_KEYBOARD);
        return injectEvent(event, displayId, injectMode);
    }

    public static boolean pressReleaseKeycode(int keyCode, int displayId, int injectMode) {
        return injectKeyEvent(KeyEvent.ACTION_DOWN, keyCode, 0, 0, displayId, injectMode)
                && injectKeyEvent(KeyEvent.ACTION_UP, keyCode, 0, 0, displayId, injectMode);
    }

    public static boolean isScreenOn() {
        return ServiceManager.getPowerManager().isScreenOn();
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

    public static boolean setClipboardText(String text) {
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

        return clipboardManager.setText(text);
    }

    /**
     * @param mode one of the {@code POWER_MODE_*} constants
     */
    public static boolean setScreenPowerMode(int mode) {
        boolean applyToMultiPhysicalDisplays = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;

        if (applyToMultiPhysicalDisplays
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                && Build.BRAND.equalsIgnoreCase("honor")
                && SurfaceControl.hasGetBuildInDisplayMethod()) {
            // Workaround for Honor devices with Android 14:
            //  - <https://github.com/Genymobile/scrcpy/issues/4823>
            //  - <https://github.com/Genymobile/scrcpy/issues/4943>
            applyToMultiPhysicalDisplays = false;
        }

        if (applyToMultiPhysicalDisplays) {
            // On Android 14, these internal methods have been moved to DisplayControl
            boolean useDisplayControl =
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE && !SurfaceControl.hasGetPhysicalDisplayIdsMethod();

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
        assert displayId != DISPLAY_ID_NONE;

        if (!isScreenOn()) {
            return true;
        }
        return pressReleaseKeycode(KeyEvent.KEYCODE_POWER, displayId, Device.INJECT_MODE_ASYNC);
    }

    /**
     * Disable auto-rotation (if enabled), set the screen rotation and re-enable auto-rotation (if it was enabled).
     */
    public static void rotateDevice(int displayId) {
        assert displayId != DISPLAY_ID_NONE;

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
        assert displayId != DISPLAY_ID_NONE;

        if (displayId == 0) {
            return ServiceManager.getWindowManager().getRotation();
        }

        DisplayInfo displayInfo = ServiceManager.getDisplayManager().getDisplayInfo(displayId);
        return displayInfo.getRotation();
    }
}
