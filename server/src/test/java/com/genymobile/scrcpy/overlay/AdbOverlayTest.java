package com.genymobile.scrcpy.overlay;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

public class AdbOverlayTest {

    @Rule
    public TemporaryFolder tempFolder = new TemporaryFolder();

    @Test
    public void testProcPollingDetectsAndCompletesAdbShellCommand() throws Exception {
        File procDir = tempFolder.newFolder("proc");
        File logFile = new File(tempFolder.getRoot(), "overlay.log");

        File adbdDir = new File(procDir, "100");
        adbdDir.mkdirs();
        writeString(new File(adbdDir, "stat"), "100 (adbd) S 1 100 100 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 10 0 0");
        writeBytes(new File(adbdDir, "cmdline"), "adbd\0".getBytes(StandardCharsets.UTF_8));

        AdbCommandMonitor monitor = new AdbCommandMonitor(5, procDir, logFile, 9999);
        monitor.initializeBaseline();

        long t0 = 1_700_000_000_000L;
        assertNull(monitor.getActiveEvent(t0));

        File childDir = new File(procDir, "250");
        childDir.mkdirs();
        writeString(new File(childDir, "stat"), "250 (sh) S 100 250 250 0 -1 0 0 0 0 0 0 0 0 0 20 0 1 0 50 0 0");
        writeBytes(
                new File(childDir, "cmdline"),
                "sh\0-c\0input keyevent 24\0".getBytes(StandardCharsets.UTF_8));

        assertTrue(monitor.pollOnce(t0 + 50));
        AdbOverlayEvent running = monitor.getActiveEvent(t0 + 50);
        assertNotNull(running);
        assertEquals("adb shell input keyevent 24", running.getCommand());
        assertEquals(AdbOverlayEvent.Status.RUNNING, running.getStatus());

        deleteRecursive(childDir);
        assertTrue(monitor.pollOnce(t0 + 120));
        AdbOverlayEvent completed = monitor.getActiveEvent(t0 + 120);
        assertNotNull(completed);
        assertEquals(AdbOverlayEvent.Status.SUCCESS, completed.getStatus());
        assertEquals(70L, completed.getDurationMs());

        assertNull(monitor.getActiveEvent(t0 + 120 + 5001));
    }

    @Test
    public void testLogFileCapturesOutputAndExitCode() throws Exception {
        File procDir = tempFolder.newFolder("proc2");
        File logFile = new File(tempFolder.getRoot(), "scrcpy_adb_overlay.log");

        AdbCommandMonitor monitor = new AdbCommandMonitor(5, procDir, logFile, 9999);
        monitor.initializeBaseline();

        long t0 = 1_700_000_010_000L;
        writeString(
                logFile,
                "START|" + t0 + "|adb shell pm list packages\n"
                        + "END|" + (t0 + 85) + "|1|85|adb shell pm list packages|Error: access denied\\n\n");

        assertTrue(monitor.pollOnce(t0 + 90));
        AdbOverlayEvent event = monitor.getActiveEvent(t0 + 90);
        assertNotNull(event);
        assertEquals("adb shell pm list packages", event.getCommand());
        assertEquals(AdbOverlayEvent.Status.ERROR, event.getStatus());
        assertEquals(1, event.getExitCode());
        assertEquals(85L, event.getDurationMs());
        assertEquals("Error: access denied", event.getOutput());
    }

    @Test
    public void testCommandExtractionAndMutationClassification() {
        assertEquals(
                "settings get global device_name",
                AdbCommandMonitor.extractExecutableShellCommand(
                        Arrays.asList("sh", "-c", "settings get global device_name")));
        assertEquals(
                "/system/bin/cmd settings get global device_name",
                AdbCommandMonitor.extractExecutableShellCommand(
                        Arrays.asList("/system/bin/cmd", "settings", "get", "global", "device_name")));

        assertFalse(AdbCommandMonitor.isMutatingShellCommand("settings get global device_name"));
        assertFalse(AdbCommandMonitor.isMutatingShellCommand("getprop ro.product.model"));
        assertFalse(AdbCommandMonitor.isMutatingShellCommand("ls /nonexistent"));
        assertTrue(AdbCommandMonitor.isMutatingShellCommand("input keyevent 24"));
        assertTrue(AdbCommandMonitor.isMutatingShellCommand("settings put global device_name MyPixel"));
        assertTrue(AdbCommandMonitor.isMutatingShellCommand("am start -n com.example/.Main"));

        List<AdbOverlayEvent> events = monitorForMultiTest();
        assertEquals(2, events.size());
    }

    private List<AdbOverlayEvent> monitorForMultiTest() {
        AdbCommandMonitor monitor = new AdbCommandMonitor(5);
        long t0 = 1_700_000_020_000L;
        monitor.parseLogLine("END|" + t0 + "|0|12|input keyevent 24|OK", t0);
        monitor.parseLogLine("END|" + (t0 + 20L) + "|0|15|input keyevent 25|OK", t0 + 20L);
        return monitor.getActiveEvents(t0 + 50L);
    }

    private static void writeString(File file, String content) throws Exception {
        writeBytes(file, content.getBytes(StandardCharsets.UTF_8));
    }

    private static void writeBytes(File file, byte[] bytes) throws Exception {
        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(bytes);
        }
    }

    private static void deleteRecursive(File file) {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursive(child);
                }
            }
        }
        file.delete();
    }
}
