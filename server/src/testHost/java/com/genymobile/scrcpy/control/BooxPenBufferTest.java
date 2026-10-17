package com.genymobile.scrcpy.control;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Actual worker loop, host-only streams: no Android services or digitizer access. */
public final class BooxPenBufferTest {
    private static final class CountingInput extends InputStream {
        private final ByteArrayInputStream source;
        private final int fragment;
        private int calls;

        CountingInput(byte[] data, int fragment) {
            source = new ByteArrayInputStream(data);
            this.fragment = fragment;
        }

        @Override
        public int read() {
            ++calls;
            return source.read();
        }

        @Override
        public int read(byte[] bytes, int offset, int length) {
            ++calls;
            return source.read(bytes, offset, Math.min(length, fragment));
        }
    }

    private static BooxPen pen(OutputStream out) {
        return new BooxPen(out, true, 0, 100, 0, 200, 4095, 20, 30, 48, 0);
    }

    private static byte[] packet(int action, float pressure) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(bytes);
        out.writeInt(action);
        out.writeLong(-4);
        out.writeFloat(.25f);
        out.writeFloat(.75f);
        out.writeFloat(pressure);
        return bytes.toByteArray();
    }

    private static void check(boolean condition, String message) {
        if (!condition) {
            throw new AssertionError(message);
        }
    }

    private static void noFillWait() throws Exception {
        CountDownLatch written = new CountDownLatch(1);
        AtomicReference<Throwable> failure = new AtomicReference<>();
        try (PipedInputStream input = new PipedInputStream()) {
            PipedOutputStream sender = new PipedOutputStream(input);
            OutputStream events = new OutputStream() {
                @Override
                public void write(int value) {
                    written.countDown();
                }

                @Override
                public void write(byte[] bytes, int offset, int length) {
                    written.countDown();
                }
            };
            Thread worker = new Thread(() -> {
                try {
                    BooxPenSession.run(input, pen(events), () -> 1L);
                } catch (Throwable e) {
                    failure.set(e);
                }
            }, "boox-buffer-test");
            worker.setDaemon(true);
            worker.start();
            try {
                sender.write(packet(0, .2f));
                sender.flush();
                check(written.await(2, TimeUnit.SECONDS), "DOWN must reach output before UP, EOF or a full input buffer");
                sender.write(packet(1, 0));
                sender.flush();
            } finally {
                sender.close();
                worker.join(2000);
            }
            check(!worker.isAlive() && failure.get() == null, "Live pipe must terminate cleanly: " + failure.get());
        }
    }

    public static void main(String[] args) throws Exception {
        ByteArrayOutputStream wire = new ByteArrayOutputStream();
        ByteArrayOutputStream expected = new ByteArrayOutputStream();
        BooxPen direct = pen(expected);
        for (int i = 0; i < 32; ++i) {
            int action = i == 0 ? 0 : 2;
            float pressure = (i + 1) / 32f;
            wire.write(packet(action, pressure));
            direct.inject(action, -4, .25f, .75f, pressure, 1L);
        }
        direct.close();
        byte[] payload = wire.toByteArray();
        for (int fragment : new int[] {Integer.MAX_VALUE, 1, 7}) {
            CountingInput input = new CountingInput(payload, fragment);
            ByteArrayOutputStream actual = new ByteArrayOutputStream();
            BooxPenSession.run(input, pen(actual), () -> 1L);
            check(Arrays.equals(expected.toByteArray(), actual.toByteArray()), "Buffering changed frame bytes or EOF release");
            if (fragment == Integer.MAX_VALUE) {
                System.out.println("BooxBuffer: 32 packets, underlying input read calls=" + input.calls);
                check(input.calls <= 3, "Queued packets should use bulk reads, observed=" + input.calls);
            }
        }
        for (int length = 1; length < 24; ++length) {
            ByteArrayOutputStream partial = new ByteArrayOutputStream();
            partial.write(packet(0, .5f));
            partial.write(packet(2, .8f), 0, length);
            ByteArrayOutputStream actual = new ByteArrayOutputStream();
            boolean failed = false;
            try {
                BooxPenSession.run(new ByteArrayInputStream(partial.toByteArray()), pen(actual), () -> 1L);
            } catch (IOException e) {
                failed = true;
            }
            ByteArrayOutputStream expectedRelease = new ByteArrayOutputStream();
            try (BooxPen reference = pen(expectedRelease)) {
                reference.inject(0, -4, .25f, .75f, .5f, 1L);
            }
            check(failed && Arrays.equals(actual.toByteArray(), expectedRelease.toByteArray()),
                    "Truncated packet must fail and release exactly once at byte " + length);
        }
        noFillWait();
        System.out.println("BooxBuffer: exact frames, fragmented input, 23 truncation offsets and immediate DOWN passed");
    }
}
