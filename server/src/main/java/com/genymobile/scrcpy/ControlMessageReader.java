package com.genymobile.scrcpy;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class ControlMessageReader {

    static final int INJECT_KEYCODE_PAYLOAD_LENGTH = 13;
    static final int INJECT_TOUCH_EVENT_PAYLOAD_LENGTH = 27;
    static final int INJECT_SCROLL_EVENT_PAYLOAD_LENGTH = 20;
    static final int BACK_OR_SCREEN_ON_LENGTH = 1;
    static final int SET_SCREEN_POWER_MODE_PAYLOAD_LENGTH = 1;
    static final int GET_CLIPBOARD_LENGTH = 1;
    static final int SET_CLIPBOARD_FIXED_PAYLOAD_LENGTH = 9;

    private static final int MESSAGE_MAX_SIZE = 1 << 18; // 256k

    public static final int CLIPBOARD_TEXT_MAX_LENGTH = MESSAGE_MAX_SIZE - 14; // type: 1 byte; sequence: 8 bytes; paste flag: 1 byte; length: 4 bytes
    public static final int INJECT_TEXT_MAX_LENGTH = 300;

    private final byte[] rawBuffer = new byte[MESSAGE_MAX_SIZE];
    private final ByteBuffer buffer = ByteBuffer.wrap(rawBuffer);

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
            case ControlMessage.TYPE_BACK_OR_SCREEN_ON:
                msg = parseBackOrScreenOnEvent();
                break;
            case ControlMessage.TYPE_GET_CLIPBOARD:
                msg = parseGetClipboard();
                break;
            case ControlMessage.TYPE_SET_CLIPBOARD:
                msg = parseSetClipboard();
                break;
            case ControlMessage.TYPE_SET_SCREEN_POWER_MODE:
                msg = parseSetScreenPowerMode();
                break;
            case ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL:
            case ControlMessage.TYPE_EXPAND_SETTINGS_PANEL:
            case ControlMessage.TYPE_COLLAPSE_PANELS:
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
        int action = Binary.toUnsigned(buffer.get());
        int keycode = buffer.getInt();
        int repeat = buffer.getInt();
        int metaState = buffer.getInt();
        return ControlMessage.createInjectKeycode(action, keycode, repeat, metaState);
    }

    private String parseString() {
        if (buffer.remaining() < 4) {
            return null;
        }
        int len = buffer.getInt();
        if (buffer.remaining() < len) {
            return null;
        }
        int position = buffer.position();
        // Move the buffer position to consume the text
        buffer.position(position + len);
        return new String(rawBuffer, position, len, StandardCharsets.UTF_8);
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
        int action = Binary.toUnsigned(buffer.get());
        long pointerId = buffer.getLong();
        Position position = readPosition(buffer);
        float pressure = Binary.u16FixedPointToFloat(buffer.getShort());
        int buttons = buffer.getInt();
        return ControlMessage.createInjectTouchEvent(action, pointerId, position, pressure, buttons);
    }

    private ControlMessage parseInjectScrollEvent() {
        if (buffer.remaining() < INJECT_SCROLL_EVENT_PAYLOAD_LENGTH) {
            return null;
        }
        Position position = readPosition(buffer);
        float hScroll = Binary.i16FixedPointToFloat(buffer.getShort());
        float vScroll = Binary.i16FixedPointToFloat(buffer.getShort());
        int buttons = buffer.getInt();
        return ControlMessage.createInjectScrollEvent(position, hScroll, vScroll, buttons);
    }

    private ControlMessage parseBackOrScreenOnEvent() {
        if (buffer.remaining() < BACK_OR_SCREEN_ON_LENGTH) {
            return null;
        }
        int action = Binary.toUnsigned(buffer.get());
        return ControlMessage.createBackOrScreenOn(action);
    }

    private ControlMessage parseGetClipboard() {
        if (buffer.remaining() < GET_CLIPBOARD_LENGTH) {
            return null;
        }
        int copyKey = Binary.toUnsigned(buffer.get());
        return ControlMessage.createGetClipboard(copyKey);
    }

    private ControlMessage parseSetClipboard() {
        if (buffer.remaining() < SET_CLIPBOARD_FIXED_PAYLOAD_LENGTH) {
            return null;
        }
        long sequence = buffer.getLong();
        boolean paste = buffer.get() != 0;
        String text = parseString();
        if (text == null) {
            return null;
        }
        return ControlMessage.createSetClipboard(sequence, text, paste);
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
        int screenWidth = Binary.toUnsigned(buffer.getShort());
        int screenHeight = Binary.toUnsigned(buffer.getShort());
        return new Position(x, y, screenWidth, screenHeight);
    }
}
