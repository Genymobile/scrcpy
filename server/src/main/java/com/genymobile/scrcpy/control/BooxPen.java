package com.genymobile.scrcpy.control;

import java.io.Closeable;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Persistent evdev encoder. Lifecycle and device ownership belong to the worker. */
public final class BooxPen implements Closeable {
    private final OutputStream out;
    private final boolean wide;
    private final ByteBuffer buffer;
    private final int minX;
    private final int maxX;
    private final int minY;
    private final int maxY;
    private final int maxPressure;
    private final int originalX;
    private final int originalY;
    private final int originalDistance;
    private final int rotation;
    private boolean active;
    private boolean closed;
    private long pointer;
    private long lastSample;

    public BooxPen(OutputStream out, boolean wide, int minX, int maxX, int minY, int maxY, int maxPressure,
            int originalX, int originalY, int originalDistance, int rotation) {
        if (out == null || minX < 0 || minY < 0 || maxX <= minX || maxY <= minY || maxPressure <= 0
                || originalX < minX || originalX > maxX || originalY < minY || originalY > maxY
                || originalDistance <= 0 || rotation < 0 || rotation > 3) {
            throw new IllegalArgumentException("Invalid BOOX calibration");
        }
        this.out = out;
        this.wide = wide;
        this.minX = minX;
        this.maxX = maxX;
        this.minY = minY;
        this.maxY = maxY;
        this.maxPressure = maxPressure;
        this.originalX = originalX;
        this.originalY = originalY;
        this.originalDistance = originalDistance;
        this.rotation = rotation;
        buffer = ByteBuffer.allocate(6 * (wide ? 24 : 16)).order(ByteOrder.nativeOrder());
    }

    public synchronized boolean inject(int action, long id, float x, float y, float pressure, long now) throws IOException {
        if (closed || (active && id != pointer)) {
            return false;
        }
        if (action == 1 || action == 3) {
            if (!active) {
                return false;
            }
            release();
            return true;
        }
        if (action != 0 && action != 2) {
            return false;
        }
        if (Float.isNaN(x) || Float.isNaN(y) || x < 0 || x > 1 || y < 0 || y > 1
                || Float.isNaN(pressure) || Float.isInfinite(pressure)) {
            release();
            return false;
        }
        if ((action == 0 && active) || (action == 2 && !active)) {
            return false;
        }
        active = true;
        pointer = id;
        lastSample = now;
        float rx;
        float ry;
        switch (rotation) {
            case 1:
                rx = y;
                ry = 1 - x;
                break;
            case 2:
                rx = 1 - x;
                ry = 1 - y;
                break;
            case 3:
                rx = 1 - y;
                ry = x;
                break;
            default:
                rx = x;
                ry = y;
        }
        int rawX = minX + Math.round(rx * (maxX - minX));
        int rawY = minY + Math.round(ry * (maxY - minY));
        int rawPressure = Math.max(1, Math.round(Math.max(0, Math.min(1, pressure)) * maxPressure));
        try {
            frame(rawX, rawY, rawPressure, 0, 1);
        } catch (IOException e) {
            try {
                release();
            } catch (IOException releaseError) {
                e.addSuppressed(releaseError);
            }
            throw e;
        }
        return true;
    }

    public synchronized void expire(long now) throws IOException {
        if (!closed && active && now - lastSample >= 1000) {
            release();
        }
    }

    private void release() throws IOException {
        if (active) {
            buffer.clear();
            event(buffer, 3, 24, 0);
            event(buffer, 1, 330, 0);
            event(buffer, 3, 25, originalDistance);
            event(buffer, 3, 0, originalX);
            event(buffer, 3, 1, originalY);
            event(buffer, 0, 0, 0);
            out.write(buffer.array());
            active = false;
        }
    }

    private void frame(int x, int y, int pressure, int distance, int touch) throws IOException {
        buffer.clear();
        event(buffer, 3, 0, x);
        event(buffer, 3, 1, y);
        event(buffer, 3, 25, distance);
        event(buffer, 3, 24, pressure);
        event(buffer, 1, 330, touch);
        event(buffer, 0, 0, 0);
        out.write(buffer.array());
    }

    private void event(ByteBuffer buffer, int type, int code, int value) {
        if (wide) {
            buffer.putLong(0).putLong(0);
        } else {
            buffer.putInt(0).putInt(0);
        }
        buffer.putShort((short) type).putShort((short) code).putInt(value);
    }

    @Override
    public synchronized void close() throws IOException {
        if (!closed) {
            try {
                release();
            } finally {
                closed = true;
                out.close();
            }
        }
    }
}
