package com.genymobile.scrcpy.overlay;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Immutable snapshot of an executed ADB command for rendering in the on-screen HUD overlay.
 */
public final class AdbOverlayEvent {

    public enum Status {
        RUNNING,
        SUCCESS,
        ERROR
    }

    private static final AtomicLong VERSION_GENERATOR = new AtomicLong(1);

    private final long version;
    private final String command;
    private final Status status;
    private final String timestampText;
    private final long startTimeMs;
    private final long completedTimeMs;
    private final long durationMs;
    private final int exitCode;
    private final String output;

    @SuppressWarnings("checkstyle:ParameterNumber")
    public AdbOverlayEvent(
            String command,
            Status status,
            String timestampText,
            long startTimeMs,
            long completedTimeMs,
            long durationMs,
            int exitCode,
            String output) {
        this.version = VERSION_GENERATOR.getAndIncrement();
        this.command = command != null ? command : "";
        this.status = status != null ? status : Status.SUCCESS;
        this.timestampText = timestampText != null ? timestampText : formatTimestamp(startTimeMs);
        this.startTimeMs = startTimeMs;
        this.completedTimeMs = completedTimeMs;
        this.durationMs = durationMs;
        this.exitCode = exitCode;
        this.output = output;
    }

    public static AdbOverlayEvent createRunning(String command, long nowMs) {
        return new AdbOverlayEvent(
                command,
                Status.RUNNING,
                formatTimestamp(nowMs),
                nowMs,
                0L,
                0L,
                0,
                null);
    }

    public AdbOverlayEvent complete(long completedAtMs, int exitCode, String output) {
        long duration = Math.max(0L, completedAtMs - this.startTimeMs);
        Status finalStatus = exitCode == 0 ? Status.SUCCESS : Status.ERROR;
        return new AdbOverlayEvent(
                this.command,
                finalStatus,
                this.timestampText,
                this.startTimeMs,
                completedAtMs,
                duration,
                exitCode,
                output != null ? output : this.output);
    }

    public long getVersion() {
        return version;
    }

    public String getCommand() {
        return command;
    }

    public Status getStatus() {
        return status;
    }

    public String getTimestampText() {
        return timestampText;
    }

    public long getStartTimeMs() {
        return startTimeMs;
    }

    public long getCompletedTimeMs() {
        return completedTimeMs;
    }

    public long getDurationMs() {
        return durationMs;
    }

    public int getExitCode() {
        return exitCode;
    }

    public String getOutput() {
        return output;
    }

    public static String formatTimestamp(long epochMs) {
        SimpleDateFormat sdf = new SimpleDateFormat("HH:mm:ss", Locale.US);
        return sdf.format(new Date(epochMs));
    }
}
