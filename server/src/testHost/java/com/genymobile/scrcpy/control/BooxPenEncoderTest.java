package com.genymobile.scrcpy.control;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Host-only encoder reuse and byte/exception contracts. Sinks snapshot every write. */
public final class BooxPenEncoderTest {
    private static void check(boolean condition, String label) {
        if (!condition) {
            throw new AssertionError(label);
        }
    }

    private static final class Sink extends OutputStream {
        final List<byte[]> frames = new ArrayList<>();
        byte[] first;
        boolean reused = true;
        boolean closed;
        int writes;
        int failThrough;
        final IOException primary = new IOException("frame failure");
        final IOException cleanup = new IOException("release failure");

        @Override
        public void write(int value) {
            throw new AssertionError("Complete frames required");
        }

        @Override
        public void write(byte[] bytes) throws IOException {
            if (first == null) {
                first = bytes;
            } else {
                reused &= first == bytes;
            }
            ++writes;
            if (writes <= failThrough) {
                throw writes == 1 ? primary : cleanup;
            }
            frames.add(bytes.clone());
        }

        @Override
        public void close() {
            closed = true;
        }
    }

    private static BooxPen pen(Sink sink, boolean wide, int rotation) {
        return new BooxPen(sink, wide, 10, 210, 20, 420, 4095, 77, 88, 48, rotation);
    }

    private static byte[] golden(boolean wide, int[][] events) {
        ByteBuffer bytes = ByteBuffer.allocate(6 * (wide ? 24 : 16)).order(ByteOrder.nativeOrder());
        for (int[] e : events) {
            // Zero timeval is part of the wire contract, including reused frames.
            bytes.position(bytes.position() + (wide ? 16 : 8));
            bytes.putShort((short) e[0]).putShort((short) e[1]).putInt(e[2]);
        }
        return bytes.array();
    }

    private static byte[] frame(boolean wide, int x, int y, int pressure) {
        return golden(wide, new int[][] {{3, 0, x}, {3, 1, y}, {3, 25, 0},
                {3, 24, pressure}, {1, 330, 1}, {0, 0, 0}});
    }

    private static byte[] release(boolean wide) {
        return golden(wide, new int[][] {{3, 24, 0}, {1, 330, 0}, {3, 25, 48},
                {3, 0, 77}, {3, 1, 88}, {0, 0, 0}});
    }

    private static void bytesAndReuse(boolean wide, int rotation) throws IOException {
        Sink sink = new Sink();
        // Quarter/three-quarter coordinates distinguish all rotations.
        int[] xs = {60, 160, 160, 60};
        int[] ys = {320, 320, 120, 120};
        BooxPen p = pen(sink, wide, rotation);
        p.inject(0, -4, .25f, .75f, .5f, 0);
        p.inject(2, -4, .25f, .75f, 2f, 1);
        p.inject(2, -4, .25f, .75f, -1f, 2);
        p.inject(3, -4, 0, 0, 0, 3);
        p.inject(0, -4, .25f, .75f, .25f, 4);
        p.close();
        byte[][] expected = {frame(wide, xs[rotation], ys[rotation], 2048),
                frame(wide, xs[rotation], ys[rotation], 4095),
                frame(wide, xs[rotation], ys[rotation], 1), release(wide),
                frame(wide, xs[rotation], ys[rotation], 1024), release(wide)};
        check(sink.frames.size() == expected.length, "Frame count");
        for (int i = 0; i < expected.length; ++i) {
            check(Arrays.equals(expected[i], sink.frames.get(i)), "Exact layout/rotation/frame " + wide + "/" + rotation + "/" + i);
        }
        check(sink.closed, "Stream closed");
        check(sink.reused, "Encoder must reuse its per-instance frame storage");
    }

    private static void failures(boolean wide) throws IOException {
        Sink sink = new Sink();
        sink.failThrough = 2;
        BooxPen p = pen(sink, wide, 0);
        try {
            p.inject(0, -4, 0, 0, 1, 0);
            throw new AssertionError("Write failure must propagate");
        } catch (IOException e) {
            check(e == sink.primary, "Original exception identity preserved");
            check(e.getSuppressed().length == 1 && e.getSuppressed()[0] == sink.cleanup, "Cleanup error suppressed");
        }
        p.close();
        check(sink.writes == 3 && sink.closed, "Close retries failed release");
        check(Arrays.equals(release(wide), sink.frames.get(0)), "Retry fully replaces failed frame bytes");

        Sink retry = new Sink();
        BooxPen active = pen(retry, wide, 0);
        active.inject(0, -4, .25f, .75f, .5f, 0);
        retry.failThrough = 2;
        try {
            active.inject(1, -4, 0, 0, 0, 1);
            throw new AssertionError("Release failure must propagate");
        } catch (IOException e) {
            check(e == retry.cleanup, "Release exception unchanged");
        }
        active.expire(1001);
        active.close();
        check(retry.writes == 3, "Watchdog retries release exactly once");
        check(Arrays.equals(frame(wide, 60, 320, 2048), retry.frames.get(0)), "Prior snapshot unchanged");
        check(Arrays.equals(release(wide), retry.frames.get(1)), "Watchdog retry release bytes");
    }

    public static void main(String[] args) throws Exception {
        // Failure semantics run even on the pre-optimization RED implementation.
        for (boolean wide : new boolean[] {false, true}) {
            failures(wide);
        }
        for (boolean wide : new boolean[] {false, true}) {
            for (int rotation = 0; rotation < 4; ++rotation) {
                bytesAndReuse(wide, rotation);
            }
        }
        System.out.println("BooxEncoder: layouts, rotations, pressure, exact release, storage reuse and I/O retries passed");
    }
}
