package com.genymobile.scrcpy.device;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.AppListProcessor;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.wrappers.ActivityManager;
import com.genymobile.scrcpy.wrappers.ClipboardManager;
import com.genymobile.scrcpy.wrappers.DisplayControl;
import com.genymobile.scrcpy.wrappers.InputManager;
import com.genymobile.scrcpy.wrappers.ServiceManager;
import com.genymobile.scrcpy.wrappers.SurfaceControl;
import com.genymobile.scrcpy.wrappers.WindowManager;

import android.annotation.SuppressLint;
import android.app.UiModeManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.app.ActivityOptions;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import java.util.List;
import java.util.Locale;

public final class Device {

    public static final int DISPLAY_ID_NONE = -1;

    public static final int POWER_MODE_OFF = SurfaceControl.POWER_MODE_OFF;
    public static final int POWER_MODE_NORMAL = SurfaceControl.POWER_MODE_NORMAL;

    public static final int INJECT_MODE_ASYNC = InputManager.INJECT_INPUT_EVENT_MODE_ASYNC;
    public static final int INJECT_MODE_WAIT_FOR_RESULT = InputManager.INJECT_INPUT_EVENT_MODE_WAIT_FOR_RESULT;
    public static final int INJECT_MODE_WAIT_FOR_FINISH = InputManager.INJECT_INPUT_EVENT_MODE_WAIT_FOR_FINISH;

    // The new display power method introduced in Android 15 does not work as expected:
    // <https://github.com/Genymobile/scrcpy/issues/5530>
    private static final boolean USE_ANDROID_15_DISPLAY_POWER = false;

    private Device() {
        // not instantiable
    }

    public static String getDeviceName() {
        return Build.MODEL;
    }

    public static boolean supportsInputEvents(int displayId) {
        // main display or any display on Android >= 10
        return displayId == 0 || Build.VERSION.SDK_INT >= AndroidVersions.API_29_ANDROID_10;
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

    public static boolean isScreenOn(int displayId) {
        assert displayId != DISPLAY_ID_NONE;
        return ServiceManager.getPowerManager().isScreenOn(displayId);
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

    public static boolean setDisplayPower(int displayId, boolean on) {
        assert displayId != Device.DISPLAY_ID_NONE;

        if (USE_ANDROID_15_DISPLAY_POWER && Build.VERSION.SDK_INT >= AndroidVersions.API_35_ANDROID_15) {
            return ServiceManager.getDisplayManager().requestDisplayPower(displayId, on);
        }

        boolean applyToMultiPhysicalDisplays = Build.VERSION.SDK_INT >= AndroidVersions.API_29_ANDROID_10;

        if (applyToMultiPhysicalDisplays
                && Build.VERSION.SDK_INT >= AndroidVersions.API_34_ANDROID_14
                && Build.BRAND.equalsIgnoreCase("honor")
                && SurfaceControl.hasGetBuildInDisplayMethod()) {
            // Workaround for Honor devices with Android 14:
            //  - <https://github.com/Genymobile/scrcpy/issues/4823>
            //  - <https://github.com/Genymobile/scrcpy/issues/4943>
            applyToMultiPhysicalDisplays = false;
        }

        int mode = on ? POWER_MODE_NORMAL : POWER_MODE_OFF;
        if (applyToMultiPhysicalDisplays) {
            // On Android 14, these internal methods have been moved to DisplayControl
            boolean useDisplayControl =
                    Build.VERSION.SDK_INT >= AndroidVersions.API_34_ANDROID_14 && !SurfaceControl.hasGetPhysicalDisplayIdsMethod();

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

        if (!isScreenOn(displayId)) {
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

    public static void startApp(Intent launchIntent, int displayId, boolean forceStop) {
        Bundle options = null;
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_26_ANDROID_8_0) {
            ActivityOptions launchOptions = ActivityOptions.makeBasic();
            launchOptions.setLaunchDisplayId(displayId);
            options = launchOptions.toBundle();
        }

        ActivityManager am = ServiceManager.getActivityManager();
        if (forceStop) {
            am.forceStopPackage(launchIntent.getComponent().getPackageName());
        }
        am.startActivity(launchIntent, options);
    }

    @SuppressLint("QueryPermissionsNeeded")
    public static List<ResolveInfo> getDrawerApps() {
        Context context = FakeContext.get();
        PackageManager pm = context.getPackageManager();
        Intent intent = new Intent(Intent.ACTION_MAIN, null);

        UiModeManager uiModeManager = (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
        intent.addCategory(uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION?
                Intent.CATEGORY_LEANBACK_LAUNCHER : Intent.CATEGORY_LAUNCHER);

        return pm.queryIntentActivities(intent, 0);
    }

    public static Intent getIntentFromAppDrawer(String query, boolean isPackageName){
        AppListProcessor appListProcessor = new AppListProcessor(isPackageName, true, query);
        query = query.toLowerCase(Locale.getDefault());
        Context context = FakeContext.get();
        PackageManager pm = context.getPackageManager();

        for (ResolveInfo drawerApp : getDrawerApps()) {
            String packageName = drawerApp.activityInfo.packageName;
            String label = drawerApp.loadLabel(pm).toString().toLowerCase(Locale.getDefault());

            if (isPackageName){
                if (packageName.equals(query)) {
                    ComponentName componentName = new ComponentName(packageName, drawerApp.activityInfo.name);
                    return new Intent().setComponent(componentName)
                            .putExtra("APP_LABEL", drawerApp.loadLabel(pm).toString());
                } else if (packageName.contains(query)){
                    appListProcessor.addPotentialMatchesPkgName(drawerApp);
                }
            } else {
                if (label.equals(query)) {
                    appListProcessor.addExactMatchesLabel(drawerApp);
                } else if (label.contains(query)){
                    appListProcessor.addPotentialMatchesAppName(drawerApp);
                }
            }
        }

        //Suggestions will show regardless if MULTIPLE_EXACT_LABELS
        Intent launchIntent = appListProcessor.getIntent(pm,false);
        if (launchIntent == null){
            Ln.w("Trying to find from list of all apps");
            return getIntentFromListOfAllApps(appListProcessor.getOrgQuery(), isPackageName);
        }
        else if (launchIntent.getBooleanExtra("MULTIPLE_EXACT_LABELS", false)) {
            //Let processResolvedLists() return a "garbage" intent if there are multiple exact labels.
            // We want to avoid redundant check. This happens if first check "launchIntent == null"
            // becomes true since getIntentFromListOfAllApps() also calls processResolvedLists().
            // This limitation can be removed to instead list possible exact label matches
            // from list of all apps also.
            return null;
        }
        return launchIntent;
    }

    @SuppressLint("QueryPermissionsNeeded")
    public static Intent getIntentFromListOfAllApps(String query, boolean isPackageName){
        AppListProcessor appListProcessor = new AppListProcessor(isPackageName, false, query);
        query = query.toLowerCase(Locale.getDefault());
        Context context = FakeContext.get();
        PackageManager pm = context.getPackageManager();

        boolean isTV = false;
        UiModeManager uiModeManager = (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
        if (uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION) {
            isTV = true;
        }

        for (ApplicationInfo appInfo : pm.getInstalledApplications(PackageManager.GET_META_DATA)) {
            String packageName = appInfo.packageName;
            String label = appInfo.loadLabel(pm).toString().toLowerCase(Locale.getDefault());
            Intent launchIntent = isTV ?
                    pm.getLeanbackLaunchIntentForPackage(packageName) :
                    pm.getLaunchIntentForPackage(packageName);

            if (isPackageName){
                if (packageName.equals(query) && launchIntent == null) {
                    Ln.e("No launch intent for " + appInfo.loadLabel(pm) + " ["+packageName+"]");
                    return null;
                } else if (packageName.equals(query)) {
                    return launchIntent.putExtra("APP_LABEL", label);
                }else if (packageName.contains(query) && launchIntent != null){
                    appListProcessor.addPotentialMatchesPkgName(pm.resolveActivity(launchIntent, 0));
                }
            } else {
                if (launchIntent == null) {
                    if (label.equals(query)){
                        Ln.w("Ignoring "+ appInfo.loadLabel(pm) + " ["+packageName+"] which has no launch intent");
                    }
                    continue;
                }
                ResolveInfo resolveInfo = pm.resolveActivity(launchIntent, 0);
                if (label.equals(query)) {
                    appListProcessor.addExactMatchesLabel(resolveInfo);
                } else if (label.contains(query)){
                    appListProcessor.addPotentialMatchesAppName(resolveInfo);
                }
            }
        }

        return appListProcessor.getIntent(pm,true);
    }

    @SuppressLint("QueryPermissionsNeeded")
    public static Intent getIntentFromClassName(String packageName, String activityName){
        Context context = FakeContext.get();
        PackageManager pm = context.getPackageManager();
        Intent intent = new Intent();
        intent.setClassName(packageName, activityName);
        List<ResolveInfo> list = pm.queryIntentActivities(intent, PackageManager.MATCH_DEFAULT_ONLY);

        if (list.size() == 1){
            intent.putExtra("APP_LABEL", list.get(0).loadLabel(pm).toString());
            Ln.i("Activity class {"+packageName+"/"+activityName+"} found");
            return intent;
        } else {
            Ln.e("Activity class {"+packageName+"/"+activityName+"} does not exist");
            return null;
        }
    }
}
