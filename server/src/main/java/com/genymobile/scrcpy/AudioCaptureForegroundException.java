package com.genymobile.scrcpy;

/**
 * Exception thrown if audio capture failed on Android 11 specifically because the running App (shell) was not in foreground.
 */
public class AudioCaptureForegroundException extends Exception {
}
