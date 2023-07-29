package com.genymobile.scrcpy;

/**
 * Exception thrown if audio/camera capture failed on Android 11 specifically because the running App (shell) was not in foreground.
 */
public class CaptureForegroundException extends Exception {
    private final String captureType;

    public CaptureForegroundException(String captureType) {
        this.captureType = captureType;
    }

    public String getCaptureType() {
        return captureType;
    }
}
