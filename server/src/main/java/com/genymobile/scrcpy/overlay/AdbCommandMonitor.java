package com.genymobile.scrcpy.overlay;

import com.genymobile.scrcpy.util.Ln;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Monitors executed ADB commands on the device via both a zero-setup {@code /proc} process
 * scanner and an optional structured log file ({@code /data/local/tmp/scrcpy_adb_overlay.log})
 * for capturing full {@code stdout}/{@code stderr} and exit codes.
 */
public final class AdbCommandMonitor {

    public static final String DEFAULT_LOG_PATH = "/data/local/tmp/scrcpy_adb_overlay.log";
    public static final int MAX_VISIBLE_EVENTS = 5;
    private static final long POLL_INTERVAL_MS = 8L;
    private static final long FADE_DURATION_MS = 400L;
    private static final long LOG_SUPPRESS_PROC_MS = 2000L;

    public interface Listener {
        void onOverlayChanged();
    }

    private final int timeoutSeconds;
    private final File procDir;
    private final File logFile;
    private final int selfPid;

    private final Set<Integer> baselinePids = new HashSet<>();
    private final Set<Integer> cachedAdbdPids = new HashSet<>();
    private final Set<Integer> internalPids = new HashSet<>();
    private final Map<Integer, TrackedProcess> activeProcesses = new HashMap<>();
    private final Map<String, Integer> activeProbeCommands = new java.util.concurrent.ConcurrentHashMap<>();
    private final Map<String, Long> recentCommandStartMs = new HashMap<>();
    private final List<AdbOverlayEvent> events = new ArrayList<>();
    private final java.util.concurrent.ExecutorService captureExecutor =
            java.util.concurrent.Executors.newCachedThreadPool(
                    r -> {
                        Thread t = new Thread(r, "adb-overlay-capture");
                        t.setDaemon(true);
                        return t;
                    });

    private volatile AdbOverlayEvent latestEvent;
    private long lastLogEventTimeMs;
    private long logFileOffset;
    private int lastActiveCount;

    private Thread monitorThread;
    private final AtomicBoolean stopped = new AtomicBoolean();
    private volatile Listener listener;

    private volatile String filterSpec;
    private final List<String> includePatterns = new ArrayList<>();
    private final List<java.util.regex.Pattern> includeRegexes = new ArrayList<>();
    private final List<String> excludePatterns = new ArrayList<>();
    private final List<java.util.regex.Pattern> excludeRegexes = new ArrayList<>();

    private static final class TrackedProcess {
        private final int pid;
        private final int ppid;
        private final String command;
        private final String executableShellCmd;
        private final long startTimeMs;
        private final AdbOverlayEvent runningEvent;
        private volatile boolean captureInFlight;
        private volatile boolean captureDone;
        private volatile int capturedExitCode;
        private volatile String capturedOutput;
        private volatile long procExitTimeMs;
        private final AtomicBoolean finalized = new AtomicBoolean(false);

        TrackedProcess(
                int pid,
                int ppid,
                String command,
                String executableShellCmd,
                long startTimeMs,
                AdbOverlayEvent runningEvent) {
            this.pid = pid;
            this.ppid = ppid;
            this.command = command;
            this.executableShellCmd = executableShellCmd;
            this.startTimeMs = startTimeMs;
            this.runningEvent = runningEvent;
        }
    }

    public AdbCommandMonitor(int timeoutSeconds) {
        this(timeoutSeconds, null, new File("/proc"), new File(DEFAULT_LOG_PATH), getCurrentPid());
    }

    public AdbCommandMonitor(int timeoutSeconds, String filterSpec) {
        this(timeoutSeconds, filterSpec, new File("/proc"), new File(DEFAULT_LOG_PATH), getCurrentPid());
    }

    public AdbCommandMonitor(int timeoutSeconds, File procDir, File logFile, int selfPid) {
        this(timeoutSeconds, null, procDir, logFile, selfPid);
    }

    public AdbCommandMonitor(int timeoutSeconds, String filterSpec, File procDir, File logFile, int selfPid) {
        this.timeoutSeconds = timeoutSeconds;
        this.procDir = procDir;
        this.logFile = logFile;
        this.selfPid = selfPid;
        setFilter(filterSpec);
    }

    public synchronized void setFilter(String filterSpec) {
        this.filterSpec = filterSpec;
        includePatterns.clear();
        includeRegexes.clear();
        excludePatterns.clear();
        excludeRegexes.clear();

        if (filterSpec == null || filterSpec.trim().isEmpty()) {
            return;
        }
        String[] tokens = filterSpec.split("[,;]");
        for (String raw : tokens) {
            String token = raw.trim();
            if (token.isEmpty()) {
                continue;
            }
            boolean exclude = false;
            if ((token.startsWith("!") || token.startsWith("-")) && token.length() > 1) {
                exclude = true;
                token = token.substring(1).trim();
            }
            if (token.isEmpty()) {
                continue;
            }
            java.util.regex.Pattern compiled = null;
            try {
                compiled = java.util.regex.Pattern.compile(token, java.util.regex.Pattern.CASE_INSENSITIVE);
            } catch (java.util.regex.PatternSyntaxException ignored) {
                // Fallback to case-insensitive substring matching
            }
            String lower = token.toLowerCase(java.util.Locale.ROOT);
            if (exclude) {
                excludePatterns.add(lower);
                excludeRegexes.add(compiled);
            } else {
                includePatterns.add(lower);
                includeRegexes.add(compiled);
            }
        }
    }

    public String getFilter() {
        return filterSpec;
    }

    public synchronized boolean matchesFilter(String cmd) {
        if (cmd == null || cmd.isEmpty()) {
            return false;
        }
        if (excludePatterns.isEmpty() && includePatterns.isEmpty()) {
            return true;
        }
        String stripped = cmd;
        if (stripped.startsWith("adb shell ")) {
            stripped = stripped.substring("adb shell ".length()).trim();
        } else if (stripped.startsWith("adb ")) {
            stripped = stripped.substring("adb ".length()).trim();
        }
        String lowerFull = cmd.toLowerCase(java.util.Locale.ROOT);
        String lowerStripped = stripped.toLowerCase(java.util.Locale.ROOT);

        for (int i = 0; i < excludePatterns.size(); i++) {
            if (matchesSinglePattern(cmd, stripped, lowerFull, lowerStripped, excludePatterns.get(i), excludeRegexes.get(i))) {
                return false;
            }
        }
        if (includePatterns.isEmpty()) {
            return true;
        }
        for (int i = 0; i < includePatterns.size(); i++) {
            if (matchesSinglePattern(cmd, stripped, lowerFull, lowerStripped, includePatterns.get(i), includeRegexes.get(i))) {
                return true;
            }
        }
        return false;
    }

    private static boolean matchesSinglePattern(
            String fullCmd,
            String strippedCmd,
            String lowerFull,
            String lowerStripped,
            String lowerSubstr,
            java.util.regex.Pattern regex) {
        if (lowerFull.contains(lowerSubstr) || lowerStripped.contains(lowerSubstr)) {
            return true;
        }
        if (regex != null) {
            if (regex.matcher(fullCmd).find() || regex.matcher(strippedCmd).find()) {
                return true;
            }
        }
        return false;
    }

    private static int getCurrentPid() {
        try {
            return android.os.Process.myPid();
        } catch (Throwable ignored) {
            return -1;
        }
    }

    public void start(Listener listener) {
        this.listener = listener;
        this.stopped.set(false);
        initializeBaseline();

        monitorThread = new Thread(this::runLoop, "adb-overlay-monitor");
        monitorThread.setDaemon(true);
        monitorThread.start();
    }

    public void stop() {
        stopped.set(true);
        if (monitorThread != null) {
            monitorThread.interrupt();
            try {
                monitorThread.join(300);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            monitorThread = null;
        }
    }

    public void initializeBaseline() {
        baselinePids.clear();
        cachedAdbdPids.clear();
        internalPids.clear();
        activeProcesses.clear();
        activeProbeCommands.clear();
        recentCommandStartMs.clear();
        synchronized (events) {
            events.clear();
        }
        latestEvent = null;
        if (procDir != null && procDir.isDirectory()) {
            String[] entries = procDir.list();
            if (entries != null) {
                for (String entry : entries) {
                    int pid = parsePid(entry);
                    if (pid > 0) {
                        baselinePids.add(pid);
                        File statFile = new File(new File(procDir, entry), "stat");
                        ProcStat stat = readProcStat(statFile);
                        if (stat != null && "adbd".equals(stat.comm)) {
                            cachedAdbdPids.add(pid);
                        }
                    }
                }
            }
        }
        if (logFile != null && logFile.isFile()) {
            logFileOffset = logFile.length();
        } else {
            logFileOffset = 0L;
        }
    }

    private void runLoop() {
        while (!stopped.get()) {
            try {
                long nowMs = System.currentTimeMillis();
                boolean changed = pollOnce(nowMs);
                List<AdbOverlayEvent> active = getActiveEvents(nowMs);
                int currentCount = active.size();
                if (changed || (currentCount != lastActiveCount)) {
                    lastActiveCount = currentCount;
                    notifyListener();
                } else if (currentCount > 0 && isAnyFading(active, nowMs)) {
                    notifyListener();
                }
                Thread.sleep(POLL_INTERVAL_MS);
            } catch (InterruptedException e) {
                break;
            } catch (Throwable t) {
                Ln.d("AdbCommandMonitor poll error: " + t.getMessage());
            }
        }
    }

    public boolean pollOnce(long nowMs) {
        boolean changed = false;
        if (pollLogFile(nowMs)) {
            changed = true;
        }
        if (pollProc(nowMs)) {
            changed = true;
        }
        return changed;
    }

    public boolean pollLogFile(long nowMs) {
        if (logFile == null || !logFile.isFile()) {
            return false;
        }
        long length = logFile.length();
        if (length < logFileOffset) {
            logFileOffset = 0L;
        }
        if (length == logFileOffset) {
            return false;
        }

        boolean updated = false;
        try (RandomAccessFile raf = new RandomAccessFile(logFile, "r")) {
            raf.seek(logFileOffset);
            String line;
            while ((line = raf.readLine()) != null) {
                String utf8Line = new String(line.getBytes(StandardCharsets.ISO_8859_1), StandardCharsets.UTF_8).trim();
                if (!utf8Line.isEmpty()) {
                    if (parseLogLine(utf8Line, nowMs)) {
                        updated = true;
                        lastLogEventTimeMs = nowMs;
                    }
                }
            }
            logFileOffset = raf.getFilePointer();
        } catch (IOException e) {
            Ln.d("Could not read ADB overlay log: " + e.getMessage());
        }
        return updated;
    }

    public boolean parseLogLine(String line, long fallbackNowMs) {
        if (line == null || line.trim().isEmpty()) {
            return false;
        }
        if (line.startsWith("START|")) {
            String[] parts = line.split("\\|", 3);
            if (parts.length >= 3) {
                long ts = parseLongSafe(parts[1], fallbackNowMs);
                String cmd = normalizeCommandDisplay(parts[2]);
                if (!matchesFilter(cmd)) {
                    return false;
                }
                Ln.i("[adb-overlay] " + cmd);
                addEvent(AdbOverlayEvent.createRunning(cmd, ts));
                return true;
            }
        } else if (line.startsWith("END|")) {
            String[] parts = line.split("\\|", 6);
            if (parts.length >= 5) {
                long ts = parseLongSafe(parts[1], fallbackNowMs);
                int exitCode = (int) parseLongSafe(parts[2], 0L);
                long durationMs = parseLongSafe(parts[3], 0L);
                String cmd = normalizeCommandDisplay(parts[4]);
                if (!matchesFilter(cmd)) {
                    return false;
                }
                String rawOutput = parts.length >= 6 ? unescapeLogOutput(parts[5]) : null;
                long startMs = Math.max(0L, ts - durationMs);
                AdbOverlayEvent.Status status =
                        exitCode == 0 ? AdbOverlayEvent.Status.SUCCESS : AdbOverlayEvent.Status.ERROR;
                AdbOverlayEvent completed = new AdbOverlayEvent(
                        cmd,
                        status,
                        AdbOverlayEvent.formatTimestamp(startMs > 0 ? startMs : ts),
                        startMs,
                        ts,
                        durationMs,
                        exitCode,
                        rawOutput);
                completeOrAddEvent(cmd, completed);
                return true;
            }
        } else {
            String cmd = normalizeCommandDisplay(line);
            if (!matchesFilter(cmd)) {
                return false;
            }
            AdbOverlayEvent completed = new AdbOverlayEvent(
                    cmd,
                    AdbOverlayEvent.Status.SUCCESS,
                    AdbOverlayEvent.formatTimestamp(fallbackNowMs),
                    fallbackNowMs,
                    fallbackNowMs,
                    0L,
                    0,
                    null);
            addEvent(completed);
            return true;
        }
        return false;
    }

    private void addEvent(AdbOverlayEvent event) {
        synchronized (events) {
            events.add(event);
            while (events.size() > MAX_VISIBLE_EVENTS * 2) {
                events.remove(0);
            }
            latestEvent = event;
        }
    }

    private void completeOrAddEvent(String cmd, AdbOverlayEvent completed) {
        synchronized (events) {
            boolean replaced = false;
            for (int i = 0; i < events.size(); i++) {
                AdbOverlayEvent existing = events.get(i);
                if (existing.getStatus() == AdbOverlayEvent.Status.RUNNING
                        && existing.getCommand().equals(cmd)) {
                    events.set(i, completed);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                events.add(completed);
                while (events.size() > MAX_VISIBLE_EVENTS * 2) {
                    events.remove(0);
                }
            }
            latestEvent = completed;
        }
    }

    private boolean hasRunningEventWithCommand(String cmd) {
        synchronized (events) {
            for (AdbOverlayEvent ev : events) {
                if (ev.getStatus() == AdbOverlayEvent.Status.RUNNING && ev.getCommand().equals(cmd)) {
                    return true;
                }
            }
            return false;
        }
    }

    public boolean pollProc(long nowMs) {
        if (procDir == null || !procDir.isDirectory()) {
            return false;
        }
        String[] entries = procDir.list();
        if (entries == null) {
            return false;
        }

        Set<Integer> currentPids = new HashSet<>(entries.length);
        List<Integer> newPids = new ArrayList<>();

        for (String entry : entries) {
            int pid = parsePid(entry);
            if (pid <= 0 || pid == selfPid) {
                continue;
            }
            currentPids.add(pid);
            if (!baselinePids.contains(pid) && !activeProcesses.containsKey(pid)) {
                newPids.add(pid);
            }
        }

        boolean changed = false;

        Iterator<Map.Entry<Integer, TrackedProcess>> it = activeProcesses.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<Integer, TrackedProcess> entry = it.next();
            int pid = entry.getKey();
            TrackedProcess proc = entry.getValue();
            if (!currentPids.contains(pid)) {
                if (proc.procExitTimeMs == 0L) {
                    proc.procExitTimeMs = nowMs;
                }
                if (!proc.captureInFlight || (nowMs - proc.procExitTimeMs > 2500L)) {
                    it.remove();
                    if (finalizeTrackedProcess(proc, nowMs)) {
                        changed = true;
                    }
                }
            }
        }

        if (newPids.isEmpty()) {
            return changed;
        }

        java.util.Collections.sort(newPids);

        Map<Integer, Integer> pidToPpid = new HashMap<>();
        Set<Integer> preExecAdbdPids = new HashSet<>();
        for (Integer pid : newPids) {
            File statFile = new File(new File(procDir, String.valueOf(pid)), "stat");
            ProcStat stat = readProcStat(statFile);
            if (stat != null) {
                pidToPpid.put(pid, stat.ppid);
                if ("adbd".equals(stat.comm)) {
                    if (stat.ppid <= 2) {
                        cachedAdbdPids.add(pid);
                    } else {
                        preExecAdbdPids.add(pid);
                    }
                }
            }
        }

        for (Integer pid : newPids) {
            int ppid = pidToPpid.containsKey(pid) ? pidToPpid.get(pid) : -1;
            if (ppid == selfPid || internalPids.contains(ppid) || activeProcesses.containsKey(ppid)
                    || isDescendantOfSelf(pid, pidToPpid)) {
                internalPids.add(pid);
                baselinePids.add(pid);
                continue;
            }

            if (!cachedAdbdPids.isEmpty() && !isDescendantOfAdbd(pid, pidToPpid, cachedAdbdPids)) {
                baselinePids.add(pid);
                continue;
            }

            List<String> args = readCmdlineArgs(new File(new File(procDir, String.valueOf(pid)), "cmdline"));
            if (args.isEmpty()) {
                baselinePids.add(pid);
                continue;
            }

            String firstArg = args.get(0);
            if (preExecAdbdPids.contains(pid) && (firstArg.endsWith("/adbd") || "adbd".equals(firstArg))) {
                // Newly forked adbd child that has not called execve() yet: leave out of baselinePids
                // so the next poll (after execve) will inspect its actual command line.
                continue;
            }

            String formatted = formatCmdlineArgs(args);
            if (formatted == null || !matchesFilter(formatted)) {
                baselinePids.add(pid);
                continue;
            }

            Integer activeProbes = activeProbeCommands.get(formatted);
            if (activeProbes != null && activeProbes > 0) {
                internalPids.add(pid);
                baselinePids.add(pid);
                continue;
            }

            if (isRealDeviceProc()) {
                Long prevStart = recentCommandStartMs.get(formatted);
                if (prevStart != null && nowMs - prevStart >= 0 && nowMs - prevStart < 150L) {
                    baselinePids.add(pid);
                    continue;
                }
                recentCommandStartMs.put(formatted, nowMs);
            }

            boolean suppressProc =
                    (nowMs - lastLogEventTimeMs <= LOG_SUPPRESS_PROC_MS)
                            || hasRunningEventWithCommand(formatted);
            AdbOverlayEvent runningEvent = null;
            if (!suppressProc) {
                Ln.i("[adb-overlay] " + formatted);
                runningEvent = AdbOverlayEvent.createRunning(formatted, nowMs);
                addEvent(runningEvent);
                changed = true;
            }

            String execCmd = extractExecutableShellCommand(args);
            TrackedProcess tracked = new TrackedProcess(pid, ppid, formatted, execCmd, nowMs, runningEvent);
            activeProcesses.put(pid, tracked);
            if (runningEvent != null && isRealDeviceProc()) {
                startProcessCapture(tracked);
            }
        }

        return changed;
    }

    private boolean isRealDeviceProc() {
        return procDir != null && "/proc".equals(procDir.getPath());
    }

    private boolean finalizeTrackedProcess(TrackedProcess proc, long finishMs) {
        if (proc.runningEvent == null || !proc.finalized.compareAndSet(false, true)) {
            return false;
        }
        if (finishMs - lastLogEventTimeMs <= LOG_SUPPRESS_PROC_MS) {
            return false;
        }
        int exitCode = proc.captureDone ? proc.capturedExitCode : 0;
        String output = proc.captureDone ? proc.capturedOutput : null;
        long effectiveEndMs = proc.procExitTimeMs > 0 ? proc.procExitTimeMs : finishMs;
        AdbOverlayEvent completed = proc.runningEvent.complete(effectiveEndMs, exitCode, output);
        synchronized (events) {
            int idx = events.indexOf(proc.runningEvent);
            if (idx >= 0) {
                events.set(idx, completed);
                if (latestEvent == proc.runningEvent) {
                    latestEvent = completed;
                }
                return true;
            }
        }
        return false;
    }

    private void startProcessCapture(TrackedProcess proc) {
        if (proc.executableShellCmd == null || proc.executableShellCmd.isEmpty()) {
            return;
        }
        proc.captureInFlight = true;
        activeProbeCommands.compute(proc.command, (k, v) -> v == null ? 1 : v + 1);
        captureExecutor.execute(
                () -> {
                    int exitCode = 0;
                    String output = null;
                    try {
                        String rawCmd = proc.executableShellCmd;
                        if (isMutatingShellCommand(rawCmd) || isContinuousCommand(rawCmd)) {
                            CaptureResult straceRes = runStraceCapture(proc.pid, 2500L);
                            if (straceRes != null) {
                                exitCode = straceRes.exitCode;
                                output = straceRes.output;
                                if (exitCode != 0 && (output == null || output.isEmpty())
                                        && !isContinuousCommand(rawCmd)) {
                                    CaptureResult errProbe = runShellProbe(rawCmd, 1500L);
                                    if (errProbe != null && errProbe.output != null && !errProbe.output.isEmpty()) {
                                        output = errProbe.output;
                                    }
                                }
                            }
                        } else {
                            CaptureResult probeRes = runShellProbe(rawCmd, 2500L);
                            if (probeRes != null) {
                                exitCode = probeRes.exitCode;
                                output = probeRes.output;
                            }
                        }
                    } catch (Throwable t) {
                        Ln.d("ADB overlay capture error: " + t.getMessage());
                    } finally {
                        proc.capturedExitCode = exitCode;
                        proc.capturedOutput = output;
                        proc.captureDone = true;
                        proc.captureInFlight = false;
                        activeProbeCommands.computeIfPresent(
                                proc.command, (k, v) -> v <= 1 ? null : v - 1);
                        if (proc.procExitTimeMs > 0 || !new File(procDir, String.valueOf(proc.pid)).exists()) {
                            long nowMs = System.currentTimeMillis();
                            if (proc.procExitTimeMs == 0L) {
                                proc.procExitTimeMs = nowMs;
                            }
                            if (finalizeTrackedProcess(proc, nowMs)) {
                                notifyListener();
                            }
                        }
                    }
                });
    }

    private static final class CaptureResult {
        private final int exitCode;
        private final String output;

        CaptureResult(int exitCode, String output) {
            this.exitCode = exitCode;
            this.output = output;
        }
    }

    private static CaptureResult runShellProbe(String rawShellCmd, long timeoutMs) {
        Process process = null;
        try {
            ProcessBuilder pb =
                    new ProcessBuilder("/system/bin/sh", "-c", "true scrcpy_adb_overlay; " + rawShellCmd);
            pb.redirectErrorStream(true);
            process = pb.start();
            final java.io.InputStream is = process.getInputStream();
            final ByteArrayOutputStream bos = new ByteArrayOutputStream();
            Thread reader =
                    new Thread(
                            () -> {
                                byte[] buf = new byte[512];
                                int n;
                                try {
                                    while ((n = is.read(buf)) != -1) {
                                        if (bos.size() < 2048) {
                                            bos.write(buf, 0, Math.min(n, 2048 - bos.size()));
                                        }
                                    }
                                } catch (IOException ignored) {
                                    // Stream closed
                                }
                            },
                            "adb-probe-reader");
            reader.setDaemon(true);
            reader.start();

            Integer exitCode = waitForProcess(process, timeoutMs);
            if (exitCode == null) {
                process.destroy();
                reader.join(100);
                String out = new String(bos.toByteArray(), StandardCharsets.UTF_8).replace("\r", "").trim();
                return new CaptureResult(0, out.isEmpty() ? null : out);
            }
            reader.join(200);
            String out = new String(bos.toByteArray(), StandardCharsets.UTF_8).replace("\r", "").trim();
            return new CaptureResult(exitCode, out.isEmpty() ? null : out);
        } catch (Throwable ignored) {
            return null;
        } finally {
            if (process != null) {
                process.destroy();
            }
        }
    }

    private static Integer waitForProcess(final Process process, long timeoutMs) {
        final int[] codeHolder = new int[] {Integer.MIN_VALUE};
        Thread waiter =
                new Thread(
                        () -> {
                            try {
                                codeHolder[0] = process.waitFor();
                            } catch (InterruptedException ignored) {
                                Thread.currentThread().interrupt();
                            }
                        },
                        "adb-process-waiter");
        waiter.setDaemon(true);
        waiter.start();
        try {
            waiter.join(timeoutMs);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        if (codeHolder[0] == Integer.MIN_VALUE) {
            waiter.interrupt();
            return null;
        }
        return codeHolder[0];
    }

    private static CaptureResult runStraceCapture(int pid, long timeoutMs) {
        File straceBin = new File("/system/bin/strace");
        if (!straceBin.canExecute() || !new File("/proc/" + pid).exists()) {
            return null;
        }
        Process process = null;
        try {
            ProcessBuilder pb =
                    new ProcessBuilder(
                            "/system/bin/strace",
                            "-p",
                            String.valueOf(pid),
                            "-f",
                            "-q",
                            "-e",
                            "trace=write,exit_group",
                            "-s",
                            "512");
            pb.redirectErrorStream(true);
            process = pb.start();
            final java.io.InputStream is = process.getInputStream();
            final ByteArrayOutputStream bos = new ByteArrayOutputStream();
            Thread reader =
                    new Thread(
                            () -> {
                                byte[] buf = new byte[512];
                                int n;
                                try {
                                    while ((n = is.read(buf)) != -1) {
                                        if (bos.size() < 16384) {
                                            bos.write(buf, 0, Math.min(n, 16384 - bos.size()));
                                        }
                                    }
                                } catch (IOException ignored) {
                                    // Stream closed
                                }
                            },
                            "adb-strace-reader");
            reader.setDaemon(true);
            reader.start();

            Integer exitCode = waitForProcess(process, timeoutMs);
            if (exitCode == null) {
                process.destroy();
            }
            reader.join(200);
            String traceText = new String(bos.toByteArray(), StandardCharsets.UTF_8);
            return parseStraceOutput(traceText);
        } catch (Throwable ignored) {
            return null;
        } finally {
            if (process != null) {
                process.destroy();
            }
        }
    }

    static CaptureResult parseStraceOutput(String traceText) {
        if (traceText == null || traceText.isEmpty()) {
            return null;
        }
        int exitCode = 0;
        boolean foundExit = false;
        StringBuilder outBuilder = new StringBuilder();
        String[] lines = traceText.split("\n");
        for (String rawLine : lines) {
            String line = rawLine.trim();
            int writeIdx = line.indexOf("write(");
            if (writeIdx >= 0) {
                String afterWrite = line.substring(writeIdx + "write(".length());
                if (afterWrite.startsWith("1, \"") || afterWrite.startsWith("2, \"")) {
                    int firstQuote = afterWrite.indexOf('"');
                    int lastQuote = afterWrite.lastIndexOf('"');
                    if (firstQuote >= 0 && lastQuote > firstQuote && outBuilder.length() < 1024) {
                        String decoded = unescapeStraceString(afterWrite.substring(firstQuote + 1, lastQuote));
                        outBuilder.append(decoded);
                    }
                }
            }
            int exitedIdx = line.indexOf("+++ exited with ");
            if (exitedIdx >= 0) {
                int start = exitedIdx + "+++ exited with ".length();
                int end = line.indexOf(" +++", start);
                if (end > start) {
                    try {
                        int code = Integer.parseInt(line.substring(start, end).trim());
                        exitCode = code < 0 ? (code & 0xff) : code;
                        foundExit = true;
                    } catch (NumberFormatException ignored) {
                        // Ignore malformed code
                    }
                }
            } else {
                int exitGroupIdx = line.indexOf("exit_group(");
                if (exitGroupIdx >= 0) {
                    int start = exitGroupIdx + "exit_group(".length();
                    int end = line.indexOf(')', start);
                    if (end > start) {
                        try {
                            int code = Integer.parseInt(line.substring(start, end).trim());
                            exitCode = code < 0 ? (code & 0xff) : code;
                            foundExit = true;
                        } catch (NumberFormatException ignored) {
                            // Ignore malformed code
                        }
                    }
                }
            }
        }
        String out = outBuilder.toString().replace("\r", "").trim();
        if (!foundExit && out.isEmpty()) {
            return null;
        }
        return new CaptureResult(exitCode, out.isEmpty() ? null : out);
    }

    private static String unescapeStraceString(String s) {
        StringBuilder sb = new StringBuilder(s.length());
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if (c == '\\' && i + 1 < s.length()) {
                char next = s.charAt(++i);
                switch (next) {
                    case 'n':
                        sb.append('\n');
                        break;
                    case 'r':
                        break;
                    case 't':
                        sb.append(' ');
                        break;
                    case '\\':
                        sb.append('\\');
                        break;
                    case '"':
                        sb.append('"');
                        break;
                    default:
                        sb.append(next);
                        break;
                }
            } else {
                sb.append(c);
            }
        }
        return sb.toString();
    }

    static String extractExecutableShellCommand(List<String> args) {
        if (args == null || args.isEmpty()) {
            return "";
        }
        String first = args.get(0);
        if (("sh".equals(first) || "/system/bin/sh".equals(first)) && args.size() >= 3 && "-c".equals(args.get(1))) {
            return String.join(" ", args.subList(2, args.size())).trim();
        }
        List<String> effective = args;
        if (("sh".equals(first) || "/system/bin/sh".equals(first))
                && args.size() >= 2
                && !args.get(1).startsWith("-")) {
            effective = args.subList(1, args.size());
        }
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < effective.size(); i++) {
            if (i > 0) {
                sb.append(' ');
            }
            String arg = effective.get(i);
            if (arg.isEmpty()) {
                sb.append("''");
            } else if (arg.matches("[a-zA-Z0-9_./:=+-]+")) {
                sb.append(arg);
            } else {
                sb.append('\'').append(arg.replace("'", "'\\''")).append('\'');
            }
        }
        return sb.toString();
    }

    static boolean isContinuousCommand(String rawShellCmd) {
        if (rawShellCmd == null) {
            return false;
        }
        String s = stripShellPrefix(rawShellCmd);
        String[] tokens = s.split("\\s+");
        if (tokens.length == 0) {
            return false;
        }
        String verb = tokens[0];
        if ("top".equals(verb) || "watch".equals(verb) || "screenrecord".equals(verb)) {
            return true;
        }
        if ("sleep".equals(verb)) {
            return true;
        }
        if ("logcat".equals(verb)) {
            return !s.contains("-d") && !s.contains("-t ") && !s.contains("--help");
        }
        if ("tail".equals(verb)) {
            return s.contains("-f") || s.contains("-F");
        }
        return false;
    }

    static boolean isMutatingShellCommand(String rawShellCmd) {
        if (rawShellCmd == null || rawShellCmd.trim().isEmpty()) {
            return false;
        }
        String cleanedRedirects =
                rawShellCmd
                        .replace("2>&1", "")
                        .replace("1>&2", "")
                        .replace(">/dev/null", "")
                        .replace("2>/dev/null", "");
        if (cleanedRedirects.contains(">")) {
            return true;
        }
        String s = stripShellPrefix(rawShellCmd);
        String[] tokens = s.split("\\s+");
        if (tokens.length == 0) {
            return false;
        }
        String verb = tokens[0];
        String sub = tokens.length >= 2 ? tokens[1] : "";

        if ("input".equals(verb)) {
            if (tokens.length < 3) {
                return false;
            }
            return "tap".equals(sub)
                    || "swipe".equals(sub)
                    || "keyevent".equals(sub)
                    || "text".equals(sub)
                    || "motionevent".equals(sub)
                    || "roll".equals(sub)
                    || "press".equals(sub)
                    || "draganddrop".equals(sub)
                    || "keycombination".equals(sub);
        }
        if ("settings".equals(verb)) {
            if ("put".equals(sub) && tokens.length >= 5) {
                return true;
            }
            if ("delete".equals(sub) && tokens.length >= 4) {
                return true;
            }
            return "reset".equals(sub) && tokens.length >= 3;
        }
        if ("am".equals(verb)) {
            if (tokens.length < 2) {
                return false;
            }
            return "start".equals(sub)
                    || "start-activity".equals(sub)
                    || "startservice".equals(sub)
                    || "start-service".equals(sub)
                    || "foreground-service".equals(sub)
                    || "start-fg-service".equals(sub)
                    || "stopservice".equals(sub)
                    || "stop-service".equals(sub)
                    || "force-stop".equals(sub)
                    || "kill".equals(sub)
                    || "kill-all".equals(sub)
                    || "broadcast".equals(sub)
                    || "instrument".equals(sub)
                    || "crash".equals(sub)
                    || "hang".equals(sub)
                    || "restart".equals(sub)
                    || "switch-user".equals(sub)
                    || "start-user".equals(sub)
                    || "unlock-user".equals(sub)
                    || "stop-user".equals(sub);
        }
        if ("pm".equals(verb)) {
            if (tokens.length < 2) {
                return false;
            }
            return "install".equals(sub)
                    || "install-create".equals(sub)
                    || "install-write".equals(sub)
                    || "install-commit".equals(sub)
                    || "install-abandon".equals(sub)
                    || "uninstall".equals(sub)
                    || "clear".equals(sub)
                    || "enable".equals(sub)
                    || "disable".equals(sub)
                    || "disable-user".equals(sub)
                    || "disable-until-used".equals(sub)
                    || "default-state".equals(sub)
                    || "suspend".equals(sub)
                    || "unsuspend".equals(sub)
                    || "hide".equals(sub)
                    || "unhide".equals(sub)
                    || "grant".equals(sub)
                    || "revoke".equals(sub)
                    || "reset-permissions".equals(sub)
                    || "create-user".equals(sub)
                    || "remove-user".equals(sub)
                    || "trim-caches".equals(sub);
        }
        if ("wm".equals(verb)) {
            return tokens.length >= 3;
        }
        if ("setprop".equals(verb) || "svc".equals(verb)) {
            return tokens.length >= 3;
        }
        if ("rm".equals(verb)
                || "rmdir".equals(verb)
                || "mv".equals(verb)
                || "cp".equals(verb)
                || "dd".equals(verb)
                || "mkdir".equals(verb)
                || "touch".equals(verb)
                || "chmod".equals(verb)
                || "chown".equals(verb)
                || "chgrp".equals(verb)
                || "ln".equals(verb)
                || "truncate".equals(verb)
                || "kill".equals(verb)
                || "pkill".equals(verb)
                || "killall".equals(verb)) {
            return tokens.length >= 2;
        }
        return "reboot".equals(verb)
                || "stop".equals(verb)
                || "start".equals(verb)
                || "setenforce".equals(verb)
                || "mount".equals(verb)
                || "umount".equals(verb);
    }

    private static String stripShellPrefix(String rawCmd) {
        String s = rawCmd.trim();
        if (s.startsWith("/system/bin/sh /system/bin/")) {
            s = s.substring("/system/bin/sh /system/bin/".length()).trim();
        } else if (s.startsWith("/system/bin/")) {
            s = s.substring("/system/bin/".length()).trim();
        }
        if (s.startsWith("cmd settings ")) {
            s = "settings " + s.substring("cmd settings ".length()).trim();
        } else if (s.startsWith("cmd activity ")) {
            s = "am " + s.substring("cmd activity ".length()).trim();
        } else if (s.startsWith("cmd package ")) {
            s = "pm " + s.substring("cmd package ".length()).trim();
        } else if (s.startsWith("cmd window ")) {
            s = "wm " + s.substring("cmd window ".length()).trim();
        } else if (s.startsWith("cmd input ")) {
            s = "input " + s.substring("cmd input ".length()).trim();
        }
        return s;
    }

    private boolean isDescendantOfSelf(int pid, Map<Integer, Integer> pidToPpid) {
        if (selfPid <= 0) {
            return false;
        }
        int cur = pid;
        for (int depth = 0; depth < 6; depth++) {
            if (cur == selfPid || internalPids.contains(cur)) {
                internalPids.add(pid);
                return true;
            }
            Integer ppid = pidToPpid.get(cur);
            if (ppid == null) {
                File statFile = new File(new File(procDir, String.valueOf(cur)), "stat");
                ProcStat stat = readProcStat(statFile);
                if (stat != null) {
                    ppid = stat.ppid;
                    pidToPpid.put(cur, ppid);
                }
            }
            if (ppid == null || ppid <= 1) {
                return false;
            }
            if (ppid == selfPid || internalPids.contains(ppid)) {
                internalPids.add(pid);
                internalPids.add(cur);
                return true;
            }
            cur = ppid;
        }
        return false;
    }

    private boolean isDescendantOfAdbd(int pid, Map<Integer, Integer> pidToPpid, Set<Integer> adbdPids) {
        int cur = pid;
        for (int depth = 0; depth < 4; depth++) {
            Integer ppid = pidToPpid.get(cur);
            if (ppid == null) {
                File statFile = new File(new File(procDir, String.valueOf(cur)), "stat");
                ProcStat stat = readProcStat(statFile);
                if (stat != null) {
                    ppid = stat.ppid;
                    pidToPpid.put(cur, ppid);
                }
            }
            if (ppid == null
                    || ppid <= 1
                    || ppid == selfPid
                    || internalPids.contains(ppid)
                    || activeProcesses.containsKey(ppid)) {
                return false;
            }
            if (adbdPids.contains(ppid)) {
                return true;
            }
            cur = ppid;
        }
        return false;
    }

    static String formatCmdlineArgs(List<String> args) {
        if (args == null || args.isEmpty()) {
            return null;
        }
        String full = String.join(" ", args).trim();
        if (isInternalOrIgnoredCommand(full)) {
            return null;
        }

        String first = args.get(0);
        if (("sh".equals(first) || "/system/bin/sh".equals(first)) && args.size() >= 3 && "-c".equals(args.get(1))) {
            String subCmd = String.join(" ", args.subList(2, args.size())).trim();
            if (isInternalOrIgnoredCommand(subCmd)) {
                return null;
            }
            return normalizeCommandDisplay(subCmd);
        }

        if (("sh".equals(first) || "/system/bin/sh".equals(first)) && args.size() == 1) {
            return null;
        }

        return normalizeCommandDisplay(full);
    }

    private static boolean isInternalOrIgnoredCommand(String cmd) {
        if (cmd == null || cmd.isEmpty()) {
            return true;
        }
        return cmd.contains("com.genymobile.scrcpy.")
                || cmd.contains("scrcpy_adb_overlay")
                || cmd.contains("scrcpy-server")
                || cmd.contains("/system/bin/idmap2")
                || cmd.startsWith("idmap2 ")
                || cmd.contains("/bin/adbd")
                || cmd.startsWith("adbd ")
                || "adbd".equals(cmd)
                || cmd.startsWith("/system/bin/strace -p ")
                || cmd.startsWith("strace -p ")
                || "getprop".equals(cmd)
                || "/system/bin/getprop".equals(cmd)
                || cmd.startsWith("printf START|")
                || cmd.startsWith("printf END|");
    }

    public static String normalizeCommandDisplay(String rawCmd) {
        if (rawCmd == null) {
            return "";
        }
        String trimmed = rawCmd.trim();
        if (trimmed.startsWith("$ ")) {
            trimmed = trimmed.substring(2).trim();
        }
        if (trimmed.startsWith("adb shell ")) {
            trimmed = trimmed.substring("adb shell ".length()).trim();
        }
        if (trimmed.startsWith("/system/bin/sh /system/bin/")) {
            trimmed = trimmed.substring("/system/bin/sh /system/bin/".length()).trim();
        } else if (trimmed.startsWith("/system/bin/")) {
            trimmed = trimmed.substring("/system/bin/".length()).trim();
        }
        if (trimmed.startsWith("cmd settings ")) {
            trimmed = "settings " + trimmed.substring("cmd settings ".length()).trim();
        } else if (trimmed.startsWith("cmd activity ")) {
            trimmed = "am " + trimmed.substring("cmd activity ".length()).trim();
        } else if (trimmed.startsWith("cmd package ")) {
            trimmed = "pm " + trimmed.substring("cmd package ".length()).trim();
        } else if (trimmed.startsWith("cmd window ")) {
            trimmed = "wm " + trimmed.substring("cmd window ".length()).trim();
        } else if (trimmed.startsWith("cmd input ")) {
            trimmed = "input " + trimmed.substring("cmd input ".length()).trim();
        }
        if (!trimmed.startsWith("adb ")) {
            return "adb shell " + trimmed;
        }
        return trimmed;
    }

    public List<AdbOverlayEvent> getActiveEvents(long nowMs) {
        List<AdbOverlayEvent> active = new ArrayList<>();
        synchronized (events) {
            Iterator<AdbOverlayEvent> it = events.iterator();
            while (it.hasNext()) {
                AdbOverlayEvent ev = it.next();
                if (ev.getStatus() == AdbOverlayEvent.Status.RUNNING || timeoutSeconds <= 0) {
                    active.add(ev);
                } else {
                    long elapsedMs = nowMs - ev.getCompletedTimeMs();
                    if (elapsedMs <= timeoutSeconds * 1000L) {
                        active.add(ev);
                    } else {
                        it.remove();
                    }
                }
            }
        }
        if (active.size() > MAX_VISIBLE_EVENTS) {
            return new ArrayList<>(active.subList(active.size() - MAX_VISIBLE_EVENTS, active.size()));
        }
        return active;
    }

    public AdbOverlayEvent getActiveEvent(long nowMs) {
        List<AdbOverlayEvent> active = getActiveEvents(nowMs);
        if (active.isEmpty()) {
            return null;
        }
        return active.get(active.size() - 1);
    }

    public float computeOverlayAlpha(AdbOverlayEvent event, long nowMs) {
        if (event == null) {
            return 0f;
        }
        if (event.getStatus() == AdbOverlayEvent.Status.RUNNING || timeoutSeconds <= 0) {
            return 1f;
        }
        long totalMs = timeoutSeconds * 1000L;
        long elapsedMs = nowMs - event.getCompletedTimeMs();
        if (elapsedMs >= totalMs) {
            return 0f;
        }
        long remainingMs = totalMs - elapsedMs;
        if (remainingMs < FADE_DURATION_MS) {
            return Math.max(0f, Math.min(1f, (float) remainingMs / (float) FADE_DURATION_MS));
        }
        return 1f;
    }

    public boolean isEventFading(AdbOverlayEvent event, long nowMs) {
        if (event == null || event.getStatus() == AdbOverlayEvent.Status.RUNNING || timeoutSeconds <= 0) {
            return false;
        }
        long totalMs = timeoutSeconds * 1000L;
        long elapsedMs = nowMs - event.getCompletedTimeMs();
        long remainingMs = totalMs - elapsedMs;
        return remainingMs > 0 && remainingMs <= FADE_DURATION_MS;
    }

    private boolean isAnyFading(List<AdbOverlayEvent> active, long nowMs) {
        for (AdbOverlayEvent ev : active) {
            if (isEventFading(ev, nowMs)) {
                return true;
            }
        }
        return false;
    }

    public int getTimeoutSeconds() {
        return timeoutSeconds;
    }

    private void notifyListener() {
        Listener l = this.listener;
        if (l != null) {
            l.onOverlayChanged();
        }
    }

    private static int parsePid(String name) {
        if (name == null || name.isEmpty()) {
            return -1;
        }
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (c < '0' || c > '9') {
                return -1;
            }
        }
        try {
            return Integer.parseInt(name);
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    private static long parseLongSafe(String s, long fallback) {
        if (s == null || s.isEmpty()) {
            return fallback;
        }
        try {
            return Long.parseLong(s.trim());
        } catch (NumberFormatException e) {
            return fallback;
        }
    }

    private static String unescapeLogOutput(String escaped) {
        if (escaped == null) {
            return null;
        }
        if (escaped.startsWith("B64:")) {
            try {
                byte[] decoded = android.util.Base64.decode(escaped.substring(4), android.util.Base64.DEFAULT);
                return new String(decoded, StandardCharsets.UTF_8).replace("\r", "").trim();
            } catch (Throwable ignored) {
                try {
                    byte[] decoded = java.util.Base64.getDecoder().decode(escaped.substring(4));
                    return new String(decoded, StandardCharsets.UTF_8).replace("\r", "").trim();
                } catch (Throwable ignored2) {
                    return "";
                }
            }
        }
        return escaped.replace("\\n", "\n").replace("\\r", "").trim();
    }

    static final class ProcStat {
        private final String comm;
        private final int ppid;

        ProcStat(String comm, int ppid) {
            this.comm = comm;
            this.ppid = ppid;
        }
    }

    static ProcStat readProcStat(File statFile) {
        String content = readFileString(statFile);
        if (content == null) {
            return null;
        }
        int lparen = content.indexOf('(');
        int rparen = content.lastIndexOf(')');
        if (lparen < 0 || rparen <= lparen || rparen + 2 >= content.length()) {
            return null;
        }
        String comm = content.substring(lparen + 1, rparen);
        String rest = content.substring(rparen + 2).trim();
        String[] fields = rest.split("\\s+");
        if (fields.length < 2) {
            return null;
        }
        try {
            int ppid = Integer.parseInt(fields[1]);
            return new ProcStat(comm, ppid);
        } catch (NumberFormatException e) {
            return null;
        }
    }

    static List<String> readCmdlineArgs(File cmdlineFile) {
        List<String> args = new ArrayList<>();
        if (cmdlineFile == null || !cmdlineFile.isFile()) {
            return args;
        }
        try (FileInputStream fis = new FileInputStream(cmdlineFile);
                ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
            byte[] buf = new byte[512];
            int n;
            while ((n = fis.read(buf)) != -1) {
                for (int i = 0; i < n; i++) {
                    if (buf[i] == 0) {
                        if (bos.size() > 0) {
                            args.add(new String(bos.toByteArray(), StandardCharsets.UTF_8));
                            bos.reset();
                        }
                    } else {
                        bos.write(buf[i]);
                    }
                }
            }
            if (bos.size() > 0) {
                args.add(new String(bos.toByteArray(), StandardCharsets.UTF_8));
            }
        } catch (IOException ignored) {
            // Process may have exited before cmdline was read
        }
        return args;
    }

    private static String readFileString(File file) {
        if (file == null || !file.isFile()) {
            return null;
        }
        try (FileInputStream fis = new FileInputStream(file);
                ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
            byte[] buf = new byte[256];
            int n;
            while ((n = fis.read(buf)) != -1) {
                bos.write(buf, 0, n);
            }
            return new String(bos.toByteArray(), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return null;
        }
    }
}
