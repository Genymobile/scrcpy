package com.genymobile.scrcpy;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class ControlMessageReader {

    static final int INJECT_KEYCODE_PAYLOAD_LENGTH = 9;
    static final int INJECT_TOUCH_EVENT_PAYLOAD_LENGTH = 27;
    static final int INJECT_SCROLL_EVENT_PAYLOAD_LENGTH = 20;
    static final int SET_SCREEN_POWER_MODE_PAYLOAD_LENGTH = 1;
    static final int SET_CLIPBOARD_FIXED_PAYLOAD_LENGTH = 1;

    public static final int CLIPBOARD_TEXT_MAX_LENGTH = 4092; // 4096 - 1 (type) - 1 (parse flag) - 2 (length)
    public static final int INJECT_TEXT_MAX_LENGTH = 300;

    private static final int RAW_BUFFER_SIZE = 4096;

    private final byte[] rawBuffer = new byte[RAW_BUFFER_SIZE];
    private final ByteBuffer buffer = ByteBuffer.wrap(rawBuffer);
    private final byte[] textBuffer = new byte[CLIPBOARD_TEXT_MAX_LENGTH];

    public ControlMessageReader() {
        // invariant: the buffer is always in "get" mode
        buffer.limit(0);
    }

    public boolean isFull() {
        return buffer.remaining() == rawBuffer.length;
    }

    public void readFrom(InputStream input) throws IOException {
        if (isFull()) {
            throw new IllegalStateException("Buffer full, call next() to consume");
        }
        buffer.compact();
        int head = buffer.position();
        int r = input.read(rawBuffer, head, rawBuffer.length - head);
        if (r == -1) {
            throw new EOFException("Controller socket closed");
        }
        buffer.position(head + r);
        buffer.flip();
    }

    public ControlMessage next() {
        if (!buffer.hasRemaining()) {
            return null;
        }
        int savedPosition = buffer.position();

        int type = buffer.get();
        ControlMessage msg;
        switch (type) {
            case ControlMessage.TYPE_INJECT_KEYCODE:
                msg = parseInjectKeycode();
                break;
            case ControlMessage.TYPE_INJECT_TEXT:
                msg = parseInjectText();
                break;
            case ControlMessage.TYPE_INJECT_TOUCH_EVENT:
                msg = parseInjectTouchEvent();
                break;
            case ControlMessage.TYPE_INJECT_SCROLL_EVENT:
                msg = parseInjectScrollEvent();
                break;
            case ControlMessage.TYPE_SET_CLIPBOARD:
                msg = parseSetClipboard();
                break;
            case ControlMessage.TYPE_SET_SCREEN_POWER_MODE:
                msg = parseSetScreenPowerMode();
                break;
            case ControlMessage.TYPE_BACK_OR_SCREEN_ON:
            case ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL:
            case ControlMessage.TYPE_COLLAPSE_NOTIFICATION_PANEL:
            case ControlMessage.TYPE_GET_CLIPBOARD:
            case ControlMessage.TYPE_ROTATE_DEVICE:
                msg = ControlMessage.createEmpty(type);
                break;
            default:
                Ln.w("Unknown event type: " + type);
                msg = null;
                break;
        }

        if (msg == null) {
            // failure, reset savedPosition
            buffer.position(savedPosition);
        }
        return msg;
    }

    private ControlMessage parseInjectKeycode() {
        if (buffer.remaining() < INJECT_KEYCODE_PAYLOAD_LENGTH) {
            return null;
        }
        int action = toUnsigned(buffer.get());
        int keycode = buffer.getInt();
        int metaState = buffer.getInt();
        return ControlMessage.createInjectKeycode(action, keycode, metaState);
    }

    private String parseString() {
        if (buffer.remaining() < 2) {
            return null;
        }
        int len = toUnsigned(buffer.getShort());
        if (buffer.remaining() < len) {
            return null;
        }
        buffer.get(textBuffer, 0, len);
        return new String(textBuffer, 0, len, StandardCharsets.UTF_8);
    }

    private ControlMessage parseInjectText() {
        String text = parseString();
        if (text == null) {
            return null;
        }
        return ControlMessage.createInjectText(text);
    }

    private ControlMessage parseInjectTouchEvent() {
        if (buffer.remaining() < INJECT_TOUCH_EVENT_PAYLOAD_LENGTH) {
            return null;
        }
        int action = toUnsigned(buffer.get());
        long pointerId = buffer.getLong();
        Position position = readPosition(buffer);
        // 16 bits fixed-point
        int pressureInt = toUnsigned(buffer.getShort());
        // convert it to a float between 0 and 1 (0x1p16f is 2^16 as float)
        float pressure = pressureInt == 0xffff ? 1f : (pressureInt / 0x1p16f);
        int buttons = buffer.getInt();
        return ControlMessage.createInjectTouchEvent(action, pointerId, position, pressure, buttons);
    }

    private ControlMessage parseInjectScrollEvent() {
        if (buffer.remaining() < INJECT_SCROLL_EVENT_PAYLOAD_LENGTH) {
            return null;
        }
        Position position = readPosition(buffer);
        int hScroll = buffer.getInt();
        int vScroll = buffer.getInt();
        return ControlMessage.createInjectScrollEvent(position, hScroll, vScroll);
    }

    private ControlMessage parseSetClipboard() {
        if (buffer.remaining() < SET_CLIPBOARD_FIXED_PAYLOAD_LENGTH) {
            return null;
        }
        boolean parse = buffer.get() != 0;
        String text = parseString();
        if (text == null) {
            return null;
        }
        return ControlMessage.createSetClipboard(text, parse);
    }

    private ControlMessage parseSetScreenPowerMode() {
        if (buffer.remaining() < SET_SCREEN_POWER_MODE_PAYLOAD_LENGTH) {
            return null;
        }
        int mode = buffer.get();
        return ControlMessage.createSetScreenPowerMode(mode);
    }

    private static Position readPosition(ByteBuffer buffer) {
        int x = buffer.getInt();
        int y = buffer.getInt();
        int screenWidth = toUnsigned(buffer.getShort());
        int screenHeight = toUnsigned(buffer.getShort());
        return new Position(x, y, screenWidth, screenHeight);
    }

    private static int toUnsigned(short value) {
        return value & 0xffff;
    }

    private static int toUnsigned(byte value) {
        return value & 0xff;
    }
}
