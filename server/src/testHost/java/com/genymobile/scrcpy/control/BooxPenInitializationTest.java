package com.genymobile.scrcpy.control;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/** Host-only discovery snapshots: waiting must never turn an unsafe state into idle. */
public final class BooxPenInitializationTest {
    private static String caps(int pressure, int distance) {
        return "add device 1: /dev/input/event7\n"
                + " name: \"onyx_emp_Wacom I2C Digitizer\"\n"
                + " ABS_X : value " + (distance == 0 ? 0 : 77) + ", min 0, max 210, fuzz 0\n"
                + " ABS_Y : value " + (distance == 0 ? 0 : 88) + ", min 0, max 420, fuzz 0\n"
                + " ABS_PRESSURE : value " + pressure + ", min 0, max 4095, fuzz 0\n"
                + " ABS_DISTANCE : value " + distance + ", min 0, max 255, fuzz 0\n";
    }

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private static void transition() throws Exception {
        String[] snapshots = {caps(0, 0), caps(120, 0), caps(0, 0), caps(0, 56)};
        int[] read = {0};
        int[] notices = {0};
        int[] sleeps = {0};
        BooxDevice result = BooxPenInitialization.awaitIdle(() -> snapshots[read[0]++],
                () -> ++notices[0], millis -> {
                    check(millis == 1000, "Polls must be paced");
                    ++sleeps[0];
                });
        check(result.distance == 56 && result.x == 77 && read[0] == 4, "Wait through contact, then return strictly idle");
        check(notices[0] == 1 && sleeps[0] == 3, "One actionable notice, not repeated log spam");
        BooxDevice ready = BooxPenInitialization.awaitIdle(() -> caps(0, 48),
                () -> { throw new AssertionError("Already initialized must not warn"); },
                millis -> { throw new AssertionError("Already initialized must not sleep"); });
        check(ready.distance == 48, "Existing initialized behavior preserved");
        // A short touch can occur entirely between polls. Do not require observing it.
        int[] next = {0};
        String[] quick = {caps(0, 0), caps(0, 56)};
        check(BooxPenInitialization.awaitIdle(() -> quick[next[0]++], () -> {}, millis -> {}).distance == 56,
                "Resume without having sampled physical contact");
    }

    private static void reject(String first, String second) throws Exception {
        int[] count = {0};
        try {
            BooxPenInitialization.awaitIdle(() -> count[0]++ == 0 ? first : second, () -> {}, millis -> {});
            throw new AssertionError("Unsafe discovery must not become an endless wait or be accepted");
        } catch (IllegalArgumentException expected) {
            check(count[0] <= 2, "Malformed discovery must fail promptly");
        }
    }

    private static void failures() throws Exception {
        reject(caps(120, 0), caps(0, 56)); // Active on launch is not the known boot-neutral state.
        reject(caps(0, 0).replace("value 0, min 0, max 210", "value 1, min 0, max 210"), caps(0, 56));
        reject(caps(0, 0), caps(0, 56).replace("event7", "event8"));
        reject(caps(0, 0), caps(0, 56).replace("I2C Digitizer", "Other Digitizer"));
        reject(caps(0, 0), caps(0, 56).replace("max 210", "max 211"));
        reject(caps(0, 0), caps(0, 56).replace("max 255", "max 256"));
        reject(caps(0, 0), caps(0, 56).replace("ABS_DISTANCE", "MISSING"));
        reject(caps(0, 0), caps(0, 56) + caps(0, 56).replace("event7", "event8"));
        reject(caps(0, 0), caps(5000, 0));
        reject(caps(0, 0), caps(-1, 0));
        reject(caps(0, 0), caps(0, -1));
        reject(caps(0, 0), caps(0, 0).replace("min 0, max 255", "min 1, max 255"));
        reject(caps(0, 0), caps(0, 0).replace("max 255", "max 0"));
        try {
            BooxPenInitialization.awaitIdle(() -> { throw new IOException("probe failure"); }, () -> {}, millis -> {});
            throw new AssertionError("Metadata I/O failure must propagate");
        } catch (IOException expected) {
            check("probe failure".equals(expected.getMessage()), "Do not mask I/O failure as pending initialization");
        }
        try {
            BooxDevice.parse(caps(0, 0));
            throw new AssertionError("Ordinary discovery must still reject distance zero");
        } catch (IllegalArgumentException expected) {
            // Waiting observes this state; it never accepts or opens it.
        }
    }

    private static void cancellation() throws Exception {
        CountDownLatch waiting = new CountDownLatch(1);
        AtomicBoolean interrupted = new AtomicBoolean();
        Thread t = new Thread(() -> {
            try {
                BooxPenInitialization.awaitIdle(() -> caps(0, 0), waiting::countDown, Thread::sleep);
            } catch (InterruptedException expected) {
                interrupted.set(true);
            } catch (IOException e) {
                throw new AssertionError(e);
            }
        }, "boox-initialization-fixture");
        t.setDaemon(true);
        t.start();
        try {
            check(waiting.await(2, TimeUnit.SECONDS), "Wait notice must be observable");
        } finally {
            t.interrupt();
            t.join(2000);
        }
        check(!t.isAlive() && interrupted.get(), "Cancellation must end pending discovery");
    }

    private static void seededAfterHandoff() throws Exception {
        BooxDevice expected = BooxDevice.parse(caps(0, 56));
        String[] samples = {caps(120, 0), caps(0, 0), caps(0, 56)};
        int[] i = {0};
        BooxDevice result = BooxPenInitialization.awaitIdle(() -> samples[i[0]++], () -> {}, millis -> {}, expected);
        check(result.isIdle() && i[0] == 3, "Known handoff may wait through resumed physical activity");
        try {
            BooxPenInitialization.awaitIdle(() -> caps(0, 56).replace("event7", "event8"), () -> {}, millis -> {}, expected);
            throw new AssertionError("Seeded wait accepted replacement");
        } catch (IllegalArgumentException accepted) {
            // Even an idle replacement must remain fatal.
        }
    }

    public static void main(String[] args) throws Exception {
        seededAfterHandoff();
        transition();
        failures();
        cancellation();
        System.out.println("BooxInitialization: boot/contact/idle, one notice, fail-closed identity/ranges and cancellation passed");
    }
}
