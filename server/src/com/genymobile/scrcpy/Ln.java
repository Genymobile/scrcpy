package com.genymobile.scrcpy;

import android.util.Log;

/**
 * Log both to Android logger (so that logs are visible in "adb logcat") and standard output/error (so that they are visible in the terminal
 * directly).
 */
public class Ln {

    private static final String TAG = "scrcpy";

    private Ln() {
        // not instantiable
    }

    public static void d(String message) {
        Log.d(TAG, message);
        System.out.println("DEBUG: " + message);
    }

    public static void i(String message) {
        Log.i(TAG, message);
        System.out.println("INFO: " + message);
    }

    public static void w(String message) {
        Log.w(TAG, message);
        System.out.println("WARN: " + message);
    }

    public static void e(String message, Throwable throwable) {
        Log.e(TAG, message, throwable);
        System.out.println("ERROR: " + message);
        throwable.printStackTrace();
    }
}
