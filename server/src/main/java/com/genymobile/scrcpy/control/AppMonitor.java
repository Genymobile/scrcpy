package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.util.Ln;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Monitor the state of a running Android application.
 * This class monitors whether a specific app is still running on a specific display
 * and notifies when the app is no longer active on that display.
 */
public class AppMonitor {
    
    private final String packageName;
    private final Runnable onAppClosed;
    private final ScheduledExecutorService executor;
    private ScheduledFuture<?> scheduledTask;
    private volatile boolean running;
    private int targetDisplayId = -1; // -1 means any display
    private TaskStackMonitor taskStackMonitor;
    private static final Pattern DISPLAY_PATTERN = Pattern.compile("Display #(\\d+)");
    // Match visible activities/tasks lines within a display block
    // Example: "* Task{73b87a7 #325 type=standard A=10150:com.android.chrome U=0 visible=true ...}"
    private static final Pattern VISIBLE_TASK_PATTERN = Pattern.compile(
        "(?:\\*\\s+)?Task\\{[^}]*A=\\d+:([^\\s]+)\\s+U=\\d+[^}]*\\bvisible=true\\b");
    
    public AppMonitor(String packageName, Runnable onAppClosed) {
        this.packageName = packageName;
        this.onAppClosed = onAppClosed;
        this.executor = Executors.newSingleThreadScheduledExecutor();
        this.running = false;
    }
    
    // Adaptive scheduling
    private static final long DELAY_FOUND_MS = 4000;
    private static final long DELAY_FAST_MS = 1000;
    private static final int MISSES_TO_CONFIRM = 3;
    private int consecutiveMisses = 0;
    
    public AppMonitor(String packageName, int displayId, Runnable onAppClosed) {
        this.packageName = packageName;
        this.targetDisplayId = displayId;
        this.onAppClosed = onAppClosed;
        this.executor = Executors.newSingleThreadScheduledExecutor();
        this.running = false;
    }
    
    /**
     * Start monitoring the application.
     */
    public void start() {
        if (running) {
            return;
        }
        
        running = true;
        Ln.i("AppMonitor started for " + packageName + (targetDisplayId >= 0 ? (" on display " + targetDisplayId) : ""));
        
        boolean listenerOk = registerTaskStackListener();
        if (listenerOk) {
            // Kick an immediate check; subsequent checks happen on task changes or backoff
            scheduleNext(0);
        } else {
            // Fallback: start polling immediately
            scheduleNext(DELAY_FAST_MS);
        }
    }
    
    /**
     * Stop monitoring the application.
     */
    public void stop() {
        if (!running) {
            return;
        }
        
        running = false;
        Ln.i("AppMonitor stopped for " + packageName);
        executor.shutdown();
        if (taskStackMonitor != null) {
            taskStackMonitor.stop();
            taskStackMonitor = null;
        }
    }
    
    /**
     * Check if the monitored app is still running on the target display.
     */
    private void checkAppStatus() {
        if (!running) {
            return;
        }
        
        try {
            // Reduce I/O: keep display headers and task lines containing the package
            String grepPkg = packageName.replace("'", "'\\''");
            String result = execCommand(
                "dumpsys activity activities | grep -E '^(Display #)|(Task).*" + grepPkg + "'"
            );
            if (result == null || result.trim().isEmpty()) {
                // Fallback without grep (in case grep is unavailable)
                result = execCommand("dumpsys activity activities");
            }
            int status = -1; // 1 found, 0 not found but target seen, -1 target not seen
            
            if (result != null && !result.trim().isEmpty()) {
                status = isAppRunningOnDisplay(result, packageName, targetDisplayId);
            }
            
            if (status == 1) {
                consecutiveMisses = 0;
                scheduleNext(DELAY_FOUND_MS);
            } else if (status == 0) {
                if (++consecutiveMisses >= MISSES_TO_CONFIRM) {
                    Ln.i("Target app no longer visible on display " + targetDisplayId + ", exiting");
                    running = false;
                    executor.shutdown();
                    onAppClosed.run();
                } else {
                    scheduleNext(DELAY_FAST_MS);
                }
            } else { // target display block not found; do not count as miss, retry fast
                scheduleNext(DELAY_FAST_MS);
            }
        } catch (Exception e) {
            Ln.w("Error checking app status: " + e.getMessage());
            // On error, retry fast to recover
            scheduleNext(DELAY_FAST_MS);
        }
    }
    
    private void scheduleNext(long delayMs) {
        if (!running) {
            return;
        }
        if (scheduledTask != null && !scheduledTask.isDone()) {
            scheduledTask.cancel(false);
        }
        scheduledTask = executor.schedule(this::checkAppStatus, delayMs, TimeUnit.MILLISECONDS);
    }

    private boolean registerTaskStackListener() {
        try {
            taskStackMonitor = new TaskStackMonitor(() -> {
                // On task stack changes, reschedule a fast check (debounced by our own logic)
                scheduleNext(DELAY_FAST_MS);
            });
            return taskStackMonitor.start();
        } catch (Throwable t) {
            Ln.w("TaskStackMonitor unavailable: " + t.getMessage());
            taskStackMonitor = null;
            return false;
        }
    }
    
    /**
     * Parse dumpsys activity activities output to check if the app is running on the target display.
     */
    private int isAppRunningOnDisplay(String dumpsysOutput, String packageName, int targetDisplayId) {
        String[] lines = dumpsysOutput.split("\n");
        int currentDisplayId = -1;
        boolean inTargetBlock = targetDisplayId < 0; // if any display, become true on first display
        boolean seenTargetDisplay = (targetDisplayId < 0);
        
        for (String line : lines) {
            line = line.trim();
            
            // Check for display information
            Matcher displayMatcher = DISPLAY_PATTERN.matcher(line);
            if (displayMatcher.find()) {
                try {
                    currentDisplayId = Integer.parseInt(displayMatcher.group(1));
                } catch (NumberFormatException e) {
                    Ln.w("Could not parse display ID: " + displayMatcher.group(1));
                }
                inTargetBlock = (targetDisplayId < 0) || (currentDisplayId == targetDisplayId);
                if (currentDisplayId == targetDisplayId) {
                    seenTargetDisplay = true;
                }
                continue;
            }
            
            // Within the relevant display section, check visible tasks
            if (currentDisplayId != -1 && inTargetBlock) {
                Matcher taskMatcher = VISIBLE_TASK_PATTERN.matcher(line);
                if (taskMatcher.find()) {
                    String taskPackage = taskMatcher.group(1);
                    if (packageName.equals(taskPackage)) {
                        return 1;
                    }
                }
            }
        }

        // If a specific display is targeted but its block is not present,
        // consider the app not visible there (counts as a miss, debounced).
        if (targetDisplayId >= 0 && !seenTargetDisplay) {
            return 0;
        }
        return 0;
    }
    
    /**
     * Execute a shell command and return the output.
     */
    private String execCommand(String command) {
        try {
            Process process = new ProcessBuilder("sh", "-c", command)
                .redirectErrorStream(true)
                .start();
            int exitCode = process.waitFor();
            
            java.io.BufferedReader reader = new java.io.BufferedReader(
                new java.io.InputStreamReader(process.getInputStream()));
            
            StringBuilder output = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                output.append(line).append("\n");
            }
            
            String result = output.toString();
            if (exitCode != 0) {
                Ln.w("Command '" + command + "' exited with code " + exitCode);
            }
            
            return result;
        } catch (Exception e) {
            Ln.w("Failed to execute command: " + command + ", error: " + e.getMessage());
            return null;
        }
    }
}
