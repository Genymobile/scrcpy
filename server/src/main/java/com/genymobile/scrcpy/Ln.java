package com.genymobile.scrcpy;

import android.util.Log;

/**
 * Log both to Android logger (so that logs are visible in "adb logcat") and standard output/error (so that they are visible in the terminal
 * directly).
 */
public final class Ln {

    private static final String TAG = "scrcpy";

    enum Level {
        DEBUG,
        INFO,
        WARN,
        ERROR;
    }

    private static final Level THRESHOLD = BuildConfig.DEBUG ? Level.DEBUG : Level.INFO;

    private Ln() {
        // not instantiable
    }

    public static boolean isEnabled(Level level) {
        return level.ordinal() >= THRESHOLD.ordinal();
    }

    public static void d(String message) {
        if (isEnabled(Level.DEBUG)) {
            Log.d(TAG, message);
            System.out.println("DEBUG: " + message);
        }
    }

    public static void i(String message) {
        if (isEnabled(Level.INFO)) {
            Log.i(TAG, message);
            System.out.println("INFO: " + message);
        }
    }

    public static void w(String message) {
        if (isEnabled(Level.WARN)) {
            Log.w(TAG, message);
            System.out.println("WARN: " + message);
        }
    }

    public static void e(String message, Throwable throwable) {
        if (isEnabled(Level.ERROR)) {
            Log.e(TAG, message, throwable);
            System.out.println("ERROR: " + message);
            throwable.printStackTrace();
        }
    }
}
