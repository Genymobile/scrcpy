package com.genymobile.scrcpy;

import android.util.Log;

/**
 * Log both to Android logger (so that logs are visible in "adb logcat") and standard output/error (so that they are visible in the terminal
 * directly).
 */
public final class Ln {

    public interface ConsolePrinter {
        void printOut(String message);
        void printErr(String message, Throwable throwable);
    }

    private static final String TAG = "scrcpy";
    private static final String PREFIX = "[server] ";

    enum Level {
        VERBOSE, DEBUG, INFO, WARN, ERROR
    }

    private static ConsolePrinter consolePrinter = new DefaultConsolePrinter();
    private static Level threshold = Level.INFO;

    private Ln() {
        // not instantiable
    }

    public static void setConsolePrinter(ConsolePrinter consolePrinter) {
        Ln.consolePrinter = consolePrinter;
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

    public static void v(String message) {
        if (isEnabled(Level.VERBOSE)) {
            Log.v(TAG, message);
            consolePrinter.printOut(PREFIX + "VERBOSE: " + message + '\n');
        }
    }

    public static void d(String message) {
        if (isEnabled(Level.DEBUG)) {
            Log.d(TAG, message);
            consolePrinter.printOut(PREFIX + "DEBUG: " + message + '\n');
        }
    }

    public static void i(String message) {
        if (isEnabled(Level.INFO)) {
            Log.i(TAG, message);
            consolePrinter.printOut(PREFIX + "INFO: " + message + '\n');
        }
    }

    public static void w(String message, Throwable throwable) {
        if (isEnabled(Level.WARN)) {
            Log.w(TAG, message, throwable);
            consolePrinter.printErr(PREFIX + "WARN: " + message + '\n', throwable);
        }
    }

    public static void w(String message) {
        w(message, null);
    }

    public static void e(String message, Throwable throwable) {
        if (isEnabled(Level.ERROR)) {
            Log.e(TAG, message, throwable);
            consolePrinter.printErr(PREFIX + "ERROR: " + message + '\n', throwable);
        }
    }

    public static void e(String message) {
        e(message, null);
    }

    public static class DefaultConsolePrinter implements ConsolePrinter {
        @Override
        public void printOut(String message) {
            System.out.print(message);
        }

        @Override
        public void printErr(String message, Throwable throwable) {
            System.err.print(message);
            if (throwable != null) {
                throwable.printStackTrace();
            }
        }
    }
}
