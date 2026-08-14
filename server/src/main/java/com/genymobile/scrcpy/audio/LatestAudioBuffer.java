package com.genymobile.scrcpy.audio;

import java.io.IOException;

/**
 * A bounded PCM queue which preserves the newest samples when the producer is
 * faster than the Android AudioTrack. Live microphone audio must prefer a short
 * gap over replaying seconds of stale speech.
 */
public final class LatestAudioBuffer {
    private final byte[] data;
    private int readPosition;
    private int size;
    private boolean closed;

    public LatestAudioBuffer(int capacity) {
        if (capacity <= 0) {
            throw new IllegalArgumentException("capacity must be positive");
        }
        data = new byte[capacity];
    }

    public synchronized void write(byte[] source) throws IOException {
        if (closed) {
            throw new IOException("Audio buffer is closed");
        }

        int sourceOffset = 0;
        int length = source.length;
        if (length >= data.length) {
            sourceOffset = length - data.length;
            length = data.length;
            readPosition = 0;
            size = 0;
        } else {
            discard(Math.max(0, size + length - data.length));
        }

        int writePosition = (readPosition + size) % data.length;
        int first = Math.min(length, data.length - writePosition);
        System.arraycopy(source, sourceOffset, data, writePosition, first);
        System.arraycopy(source, sourceOffset + first, data, 0, length - first);
        size += length;
        notifyAll();
    }

    public synchronized int read(byte[] target) throws IOException {
        while (size == 0 && !closed) {
            try {
                wait();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                throw new IOException("Interrupted while waiting for audio", e);
            }
        }
        if (size == 0) {
            return -1;
        }

        int length = Math.min(target.length, size);
        int first = Math.min(length, data.length - readPosition);
        System.arraycopy(data, readPosition, target, 0, first);
        System.arraycopy(data, 0, target, first, length - first);
        discard(length);
        return length;
    }

    public synchronized void close() {
        closed = true;
        notifyAll();
    }

    private void discard(int length) {
        readPosition = (readPosition + length) % data.length;
        size -= length;
    }
}
