package com.genymobile.scrcpy.android;

import android.view.KeyEvent;
import java.io.IOException;

final class ControlWriter {
    private final AdbTransport.AdbStream stream;

    ControlWriter(AdbTransport.AdbStream stream) {
        this.stream = stream;
    }

    synchronized void touch(int action, float x, float y, float pressure, int width, int height) throws IOException {
        byte[] data = new byte[32];
        data[0] = 2;
        data[1] = (byte) action;
        writeLongBE(data, 2, 0xfffffffffffffffeL);
        writeIntBE(data, 10, Math.round(x));
        writeIntBE(data, 14, Math.round(y));
        writeShortBE(data, 18, width);
        writeShortBE(data, 20, height);
        writeShortBE(data, 22, Math.round(Math.max(0, Math.min(1, pressure)) * 65535));
        writeIntBE(data, 24, 0);
        writeIntBE(data, 28, 0);
        stream.write(data);
    }

    synchronized void sendBack() throws IOException {
        sendKey(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0, 0);
        sendKey(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK, 0, 0);
    }

    synchronized void wake() throws IOException {
        stream.write(new byte[]{10, 1});
    }

    private void sendKey(int action, int keycode, int repeat, int metaState) throws IOException {
        byte[] data = new byte[14];
        data[0] = 0;
        data[1] = (byte) action;
        writeIntBE(data, 2, keycode);
        writeIntBE(data, 6, repeat);
        writeIntBE(data, 10, metaState);
        stream.write(data);
    }

    private static void writeShortBE(byte[] data, int offset, int value) {
        data[offset] = (byte) (value >> 8);
        data[offset + 1] = (byte) value;
    }

    private static void writeIntBE(byte[] data, int offset, int value) {
        data[offset] = (byte) (value >> 24);
        data[offset + 1] = (byte) (value >> 16);
        data[offset + 2] = (byte) (value >> 8);
        data[offset + 3] = (byte) value;
    }

    private static void writeLongBE(byte[] data, int offset, long value) {
        for (int i = 7; i >= 0; --i) {
            data[offset + 7 - i] = (byte) (value >> (i * 8));
        }
    }
}
