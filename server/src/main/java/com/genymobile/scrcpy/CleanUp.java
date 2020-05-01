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

    public static void configure(boolean disableShowTouches) throws IOException {
        boolean needProcess = disableShowTouches;
        if (needProcess) {
            startProcess(disableShowTouches);
        } else {
            // There is no additional clean up to do when scrcpy dies
            unlinkSelf();
        }
    }

    private static void startProcess(boolean disableShowTouches) throws IOException {
        String[] cmd = {"app_process", "/", CleanUp.class.getName(), String.valueOf(disableShowTouches)};

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

        if (disableShowTouches) {
            ServiceManager serviceManager = new ServiceManager();
            try (ContentProvider settings = serviceManager.getActivityManager().createSettingsProvider()) {
                Ln.i("Disabling \"show touches\"");
                settings.putValue(ContentProvider.TABLE_SYSTEM, "show_touches", "0");
            }
        }
    }
}
