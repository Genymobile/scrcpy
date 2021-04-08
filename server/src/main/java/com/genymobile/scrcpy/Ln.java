package com.genymobile.scrcpy;

import android.util.Log;

/**
 * Log both to Android logger (so that logs are visible in "adb logcat") and standard output/error (so that they are visible in the terminal
 * directly).
 */
public final class Ln {

    private static final String TAG = "scrcpy";
    private static final String PREFIX = "[server] ";

    enum Level {
        DEBUG, INFO, WARN, ERROR
    }

    private static Level threshold = Level.INFO;

    private Ln() {
        // not instantiable
    }

    /**
     * Initialize the log level.
     * <p>
     * Must be called before starting any new thread.
     *
     * @param level the log level
     */
    public static void initLogLevel(Level level) {
        threshold = level;
    }

    public static boolean isEnabled(Level level) {
        return level.ordinal() >= threshold.ordinal();
    }

    public static void d(String message) {
        if (isEnabled(Level.DEBUG)) {
            Log.d(TAG, message);
            System.out.println(PREFIX + "DEBUG: " + message);
        }
    }

    public static void i(String message) {
        if (isEnabled(Level.INFO)) {
            Log.i(TAG, message);
            System.out.println(PREFIX + "INFO: " + message);
        }
    }

    public static void w(String message) {
        if (isEnabled(Level.WARN)) {
            Log.w(TAG, message);
            System.out.println(PREFIX + "WARN: " + message);
        }
    }

    public static void e(String message, Throwable throwable) {
        if (isEnabled(Level.ERROR)) {
            Log.e(TAG, message, throwable);
            System.out.println(PREFIX + "ERROR: " + message);
            if (throwable != null) {
                throwable.printStackTrace();
            }
        }
    }

    public static void e(String message) {
        e(message, null);
    }
}
