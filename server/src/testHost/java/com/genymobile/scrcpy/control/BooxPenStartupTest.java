package com.genymobile.scrcpy.control;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public final class BooxPenStartupTest {
    private static void check(boolean ok, String why) {
        if (!ok) {
            throw new AssertionError(why);
        }
    }

    private static void await(CountDownLatch latch) throws InterruptedException {
        check(latch.await(2, TimeUnit.SECONDS), "Fixture timed out");
    }

    private static final class Sink implements BooxPenStartup.Writer {
        int injected;
        int closed;

        @Override
        public void inject(int action, long pointer, float x, float y, float pressure) {
            ++injected;
        }

        @Override
        public void close() {
            ++closed;
        }
    }

    private static void pendingAndReady() throws Exception {
        Sink sink = new Sink();
        AtomicInteger failures = new AtomicInteger();
        CountDownLatch entered = new CountDownLatch(1);
        CountDownLatch release = new CountDownLatch(1);
        BooxPenStartup startup = new BooxPenStartup(() -> {
            entered.countDown();
            release.await();
            return sink;
        }, error -> failures.incrementAndGet());
        startup.start();
        try {
            await(entered);
            for (int i = 0; i < 1000; ++i) {
                check(!startup.inject(0, -4, 0, 0, 0.5f), "Pending pen must be discarded");
            }
            check(sink.injected == 0, "No writer activity before ready");
            release.countDown();
            startup.join();
            check(sink.injected == 0, "No replay of pending events");
            check(startup.inject(0, -4, 1, 2, 0.8f), "Ready writer must receive new input");
            check(sink.injected == 1, "Exactly one fresh event");
        } finally {
            startup.close();
            startup.join();
        }
        startup.close();
        check(sink.closed == 1 && failures.get() == 0, "Idempotent ownership cleanup");
        check(!startup.inject(0, -4, 0, 0, 1), "No input after stop");
    }

    private static void cancel(boolean lateWriter) throws Exception {
        Sink sink = new Sink();
        AtomicInteger failures = new AtomicInteger();
        CountDownLatch entered = new CountDownLatch(1);
        BooxPenStartup startup = new BooxPenStartup(() -> {
            entered.countDown();
            try {
                new CountDownLatch(1).await();
            } catch (InterruptedException e) {
                if (!lateWriter) {
                    throw e;
                }
                return sink; // A factory may complete concurrently with cancellation.
            }
            throw new AssertionError("Unreachable");
        }, error -> failures.incrementAndGet());
        startup.start();
        await(entered);
        startup.close();
        startup.join();
        check(sink.closed == (lateWriter ? 1 : 0), "Late result must close, not publish");
        check(failures.get() == 0, "Cancellation is not a fatal startup error");
        check(!startup.inject(0, -4, 0, 0, 1), "Cancelled writer must not publish");
    }

    private static void fatal() throws Exception {
        AtomicInteger failures = new AtomicInteger();
        BooxPenStartup startup = new BooxPenStartup(() -> { throw new IOException("broken discovery"); }, error -> {
            check("broken discovery".equals(error.getMessage()), "Preserve actual error");
            failures.incrementAndGet();
        });
        startup.start();
        startup.join();
        check(failures.get() == 1, "Wake blocked receiver exactly once on fatal startup");
        check(!startup.inject(0, -4, 0, 0, 1), "Fatal writer must never receive input");
        startup.close();
        BooxPenStartup stopped = new BooxPenStartup(() -> { throw new AssertionError("Stopped factory ran"); }, error -> {});
        stopped.close();
        stopped.start();
        stopped.join();
    }

    public static void main(String[] args) throws Exception {
        pendingAndReady();
        cancel(false);
        cancel(true);
        fatal();
        System.out.println("BooxStartupLifecycle: pending drop/no replay, ready routing, cancellation, late close and fatal wake passed");
    }
}
