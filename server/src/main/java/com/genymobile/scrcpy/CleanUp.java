package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ContentProvider;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import java.io.File;
import java.io.IOException;

/**
 * Handle the cleanup of scrcpy, even if the main process is killed.
 * <p>
 * This is useful to restore some state when scrcpy is closed, even on device disconnection (which kills the scrcpy process).
 */
public final class CleanUp {

    public static final String SERVER_PATH = "/data/local/tmp/scrcpy-server.jar";

    private CleanUp() {
        // not instantiable
    }

    public static void configure(boolean disableShowTouches, int restoreStayOn, boolean restoreNormalPowerMode, boolean powerOffScreen, int displayId)
            throws IOException {
        boolean needProcess = disableShowTouches || restoreStayOn != -1 || restoreNormalPowerMode || powerOffScreen;
        if (needProcess) {
            startProcess(disableShowTouches, restoreStayOn, restoreNormalPowerMode, powerOffScreen, displayId);
        } else {
            // There is no additional clean up to do when scrcpy dies
            unlinkSelf();
        }
    }

    private static void startProcess(boolean disableShowTouches, int restoreStayOn, boolean restoreNormalPowerMode, boolean powerOffScreen,
            int displayId) throws IOException {
        String[] cmd = {"app_process", "/", CleanUp.class.getName(), String.valueOf(disableShowTouches), String.valueOf(
                restoreStayOn), String.valueOf(restoreNormalPowerMode), String.valueOf(powerOffScreen), String.valueOf(displayId)};

        ProcessBuilder builder = new ProcessBuilder(cmd);
        builder.environment().put("CLASSPATH", SERVER_PATH);
        builder.start();
    }

    private static void unlinkSelf() {
        try {
            new File(SERVER_PATH).delete();
        } catch (Exception e) {
            Ln.e("Could not unlink server", e);
        }
    }

    public static void main(String... args) {
        unlinkSelf();

        try {
            // Wait for the server to die
            System.in.read();
        } catch (IOException e) {
            // Expected when the server is dead
        }

        Ln.i("Cleaning up");

        boolean disableShowTouches = Boolean.parseBoolean(args[0]);
        int restoreStayOn = Integer.parseInt(args[1]);
        boolean restoreNormalPowerMode = Boolean.parseBoolean(args[2]);
        boolean powerOffScreen = Boolean.parseBoolean(args[3]);
        int displayId = Integer.parseInt(args[4]);

        if (disableShowTouches || restoreStayOn != -1) {
            ServiceManager serviceManager = new ServiceManager();
            try (ContentProvider settings = serviceManager.getActivityManager().createSettingsProvider()) {
                if (disableShowTouches) {
                    Ln.i("Disabling \"show touches\"");
                    settings.putValue(ContentProvider.TABLE_SYSTEM, "show_touches", "0");
                }
                if (restoreStayOn != -1) {
                    Ln.i("Restoring \"stay awake\"");
                    settings.putValue(ContentProvider.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(restoreStayOn));
                }
            }
        }

        if (Device.isScreenOn()) {
            if (powerOffScreen) {
                Ln.i("Power off screen");
                Device.powerOffScreen(displayId);
            } else if (restoreNormalPowerMode) {
                Ln.i("Restoring normal power mode");
                Device.setScreenPowerMode(Device.POWER_MODE_NORMAL);
            }
        }
    }
}
