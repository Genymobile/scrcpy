package com.genymobile.scrcpy.ime;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public final class ImeProtocol {
    public static final int MAGIC = 0x5343494d; // "SCIM"
    public static final int VERSION = 1;
    public static final int ACK_OK = 0;
    public static final int ACK_BUSY = 1;
    public static final int FRAME_TEXT = 1;
    public static final int FRAME_CLOSE = 2;
    public static final int FRAME_COMPOSING_TEXT = 3;
    public static final int FRAME_CURSOR_ANCHOR = 1; // IME-to-server direction
    public static final int TEXT_MAX_LENGTH = 300;

    private ImeProtocol() {
        // not instantiable
    }

    public static CursorAnchor readCursorAnchor(DataInputStream input) throws IOException {
        int frameType = input.readUnsignedByte();
        if (frameType != FRAME_CURSOR_ANCHOR) {
            throw new IOException("Unknown IME-to-server frame: " + frameType);
        }
        boolean valid = input.readBoolean();
        float x1 = input.readFloat();
        float y1 = input.readFloat();
        float x2 = input.readFloat();
        float y2 = input.readFloat();
        return new CursorAnchor(valid, x1, y1, x2, y2);
    }

    public static final class CursorAnchor {
        private final boolean valid;
        private final float x1;
        private final float y1;
        private final float x2;
        private final float y2;

        private CursorAnchor(boolean valid, float x1, float y1, float x2, float y2) {
            this.valid = valid;
            this.x1 = x1;
            this.y1 = y1;
            this.x2 = x2;
            this.y2 = y2;
        }

        public boolean isValid() {
            return valid;
        }

        public float getX1() {
            return x1;
        }

        public float getY1() {
            return y1;
        }

        public float getX2() {
            return x2;
        }

        public float getY2() {
            return y2;
        }
    }

    public static void writeHandshake(DataOutputStream output, String originalIme) throws IOException {
        byte[] originalImeBytes = originalIme.getBytes(StandardCharsets.UTF_8);
        if (originalImeBytes.length == 0 || originalImeBytes.length > 0xffff) {
            throw new IOException("Invalid input method id length: " + originalImeBytes.length);
        }
        output.writeInt(MAGIC);
        output.writeByte(VERSION);
        output.writeShort(originalImeBytes.length);
        output.write(originalImeBytes);
    }

    public static void writeText(DataOutputStream output, String text) throws IOException {
        writeTextFrame(output, FRAME_TEXT, text);
    }

    public static void writeComposingText(DataOutputStream output, String text) throws IOException {
        writeTextFrame(output, FRAME_COMPOSING_TEXT, text);
    }

    private static void writeTextFrame(DataOutputStream output, int frameType, String text) throws IOException {
        byte[] bytes = text.getBytes(StandardCharsets.UTF_8);
        if (bytes.length > TEXT_MAX_LENGTH) {
            throw new IOException("IME text payload exceeds " + TEXT_MAX_LENGTH + " bytes");
        }
        output.writeByte(frameType);
        output.writeInt(bytes.length);
        output.write(bytes);
    }

    public static void writeClose(DataOutputStream output) throws IOException {
        output.writeByte(FRAME_CLOSE);
    }
}
