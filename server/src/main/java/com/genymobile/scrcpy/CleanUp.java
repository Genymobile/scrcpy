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

    public static void configure(boolean disableShowTouches, int restoreStayOn) throws IOException {
        boolean needProcess = disableShowTouches || restoreStayOn != -1;
        if (needProcess) {
            startProcess(disableShowTouches, restoreStayOn);
        } else {
            // There is no additional clean up to do when scrcpy dies
            unlinkSelf();
        }
    }

    private static void startProcess(boolean disableShowTouches, int restoreStayOn) throws IOException {
        String[] cmd = {"app_process", "/", CleanUp.class.getName(), String.valueOf(disableShowTouches), String.valueOf(restoreStayOn)};

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
    }
}
