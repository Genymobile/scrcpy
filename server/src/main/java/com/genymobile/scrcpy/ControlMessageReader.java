package com.genymobile.scrcpy;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class ControlMessageReader {

    static final int INJECT_KEYCODE_PAYLOAD_LENGTH = 13;
    static final int INJECT_TOUCH_EVENT_PAYLOAD_LENGTH = 31;
    static final int INJECT_SCROLL_EVENT_PAYLOAD_LENGTH = 20;
    static final int BACK_OR_SCREEN_ON_LENGTH = 1;
    static final int SET_SCREEN_POWER_MODE_PAYLOAD_LENGTH = 1;
    static final int GET_CLIPBOARD_LENGTH = 1;
    static final int SET_CLIPBOARD_FIXED_PAYLOAD_LENGTH = 9;
    static final int UHID_CREATE_FIXED_PAYLOAD_LENGTH = 4;
    static final int UHID_INPUT_FIXED_PAYLOAD_LENGTH = 4;

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
        return parseEvent(buffer);
    }

    public ControlMessage parseEvent(ByteBuffer buffer) {
        if (!buffer.hasRemaining()) {
            return null;
        }
        int savedPosition = buffer.position();

        int type = buffer.get();
        ControlMessage msg;
        switch (type) {
            case ControlMessage.TYPE_INJECT_KEYCODE:
                msg = parseInjectKeycode(buffer);
                break;
            case ControlMessage.TYPE_INJECT_TEXT:
                msg = parseInjectText(buffer);
                break;
            case ControlMessage.TYPE_INJECT_TOUCH_EVENT:
                msg = parseInjectTouchEvent(buffer);
                break;
            case ControlMessage.TYPE_INJECT_SCROLL_EVENT:
                msg = parseInjectScrollEvent(buffer);
                break;
            case ControlMessage.TYPE_BACK_OR_SCREEN_ON:
                msg = parseBackOrScreenOnEvent(buffer);
                break;
            case ControlMessage.TYPE_GET_CLIPBOARD:
                msg = parseGetClipboard(buffer);
                break;
            case ControlMessage.TYPE_SET_CLIPBOARD:
                msg = parseSetClipboard(buffer);
                break;
            case ControlMessage.TYPE_SET_SCREEN_POWER_MODE:
                msg = parseSetScreenPowerMode(buffer);
                break;
            case ControlMessage.TYPE_CHANGE_STREAM_PARAMETERS:
                msg = parseChangeStreamParameters(buffer);
                break;
            case ControlMessage.TYPE_PUSH_FILE:
                msg = parsePushFile(buffer);
                break;
            case ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL:
            case ControlMessage.TYPE_EXPAND_SETTINGS_PANEL:
            case ControlMessage.TYPE_COLLAPSE_PANELS:
            case ControlMessage.TYPE_ROTATE_DEVICE:
            case ControlMessage.TYPE_OPEN_HARD_KEYBOARD_SETTINGS:
                msg = ControlMessage.createEmpty(type);
                break;
            case ControlMessage.TYPE_UHID_CREATE:
                msg = parseUhidCreate(buffer);
                break;
            case ControlMessage.TYPE_UHID_INPUT:
                msg = parseUhidInput(buffer);
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

    private ControlMessage parseChangeStreamParameters(ByteBuffer buffer) {
        int re = buffer.remaining();
        byte[] bytes = new byte[re];
        if (re > 0) {
            buffer.get(bytes, 0, re);
        }
        return ControlMessage.createChangeSteamParameters(bytes);
    }

    private ControlMessage parsePushFile(ByteBuffer buffer) {
        int re = buffer.remaining();
        byte[] bytes = new byte[re];
        if (re > 0) {
            buffer.get(bytes, 0, re);
        }
        return ControlMessage.createFilePush(bytes);
    }

    private ControlMessage parseInjectKeycode(ByteBuffer buffer) {
        if (buffer.remaining() < INJECT_KEYCODE_PAYLOAD_LENGTH) {
            return null;
        }
        int action = Binary.toUnsigned(buffer.get());
        int keycode = buffer.getInt();
        int repeat = buffer.getInt();
        int metaState = buffer.getInt();
        return ControlMessage.createInjectKeycode(action, keycode, repeat, metaState);
    }

    private int parseBufferLength(int sizeBytes) {
        assert sizeBytes > 0 && sizeBytes <= 4;
        if (buffer.remaining() < sizeBytes) {
            return -1;
        }
        int value = 0;
        for (int i = 0; i < sizeBytes; ++i) {
            value = (value << 8) | (buffer.get() & 0xFF);
        }
        return value;
    }

    private String parseString(ByteBuffer buffer) {
        int len = parseBufferLength(4);
        if (len == -1 || buffer.remaining() < len) {
            return null;
        }
        buffer.get(rawBuffer, 0, len);
        return new String(rawBuffer, 0, len, StandardCharsets.UTF_8);
    }

    private byte[] parseByteArray(int sizeBytes) {
        int len = parseBufferLength(sizeBytes);
        if (len == -1 || buffer.remaining() < len) {
            return null;
        }
        byte[] data = new byte[len];
        buffer.get(data);
        return data;
    }

    private ControlMessage parseInjectText(ByteBuffer buffer) {
        String text = parseString(buffer);
        if (text == null) {
            return null;
        }
        return ControlMessage.createInjectText(text);
    }

    private ControlMessage parseInjectTouchEvent(ByteBuffer buffer) {
        if (buffer.remaining() < INJECT_TOUCH_EVENT_PAYLOAD_LENGTH) {
            return null;
        }
        int action = Binary.toUnsigned(buffer.get());
        long pointerId = buffer.getLong();
        Position position = readPosition(buffer);
        float pressure = Binary.u16FixedPointToFloat(buffer.getShort());
        int actionButton = buffer.getInt();
        int buttons = buffer.getInt();
        return ControlMessage.createInjectTouchEvent(action, pointerId, position, pressure, actionButton, buttons);
    }

    private ControlMessage parseInjectScrollEvent(ByteBuffer buffer) {
        if (buffer.remaining() < INJECT_SCROLL_EVENT_PAYLOAD_LENGTH) {
            return null;
        }
        Position position = readPosition(buffer);
        float hScroll = Binary.i16FixedPointToFloat(buffer.getShort());
        float vScroll = Binary.i16FixedPointToFloat(buffer.getShort());
        int buttons = buffer.getInt();
        return ControlMessage.createInjectScrollEvent(position, hScroll, vScroll, buttons);
    }

    private ControlMessage parseBackOrScreenOnEvent(ByteBuffer buffer) {
        if (buffer.remaining() < BACK_OR_SCREEN_ON_LENGTH) {
            return null;
        }
        int action = Binary.toUnsigned(buffer.get());
        return ControlMessage.createBackOrScreenOn(action);
    }

    private ControlMessage parseGetClipboard(ByteBuffer buffer) {
        if (buffer.remaining() < GET_CLIPBOARD_LENGTH) {
            return null;
        }
        int copyKey = Binary.toUnsigned(buffer.get());
        return ControlMessage.createGetClipboard(copyKey);
    }

    private ControlMessage parseSetClipboard(ByteBuffer buffer) {
        if (buffer.remaining() < SET_CLIPBOARD_FIXED_PAYLOAD_LENGTH) {
            return null;
        }
        long sequence = buffer.getLong();
        boolean paste = buffer.get() != 0;
        String text = parseString(buffer);
        if (text == null) {
            return null;
        }
        return ControlMessage.createSetClipboard(sequence, text, paste);
    }

    private ControlMessage parseSetScreenPowerMode(ByteBuffer buffer) {
        if (buffer.remaining() < SET_SCREEN_POWER_MODE_PAYLOAD_LENGTH) {
            return null;
        }
        int mode = buffer.get();
        return ControlMessage.createSetScreenPowerMode(mode);
    }

    private ControlMessage parseUhidCreate(ByteBuffer buffer) {
        if (buffer.remaining() < UHID_CREATE_FIXED_PAYLOAD_LENGTH) {
            return null;
        }
        int id = buffer.getShort();
        byte[] data = parseByteArray(2);
        if (data == null) {
            return null;
        }
        return ControlMessage.createUhidCreate(id, data);
    }

    private ControlMessage parseUhidInput(ByteBuffer buffer) {
        if (buffer.remaining() < UHID_INPUT_FIXED_PAYLOAD_LENGTH) {
            return null;
        }
        int id = buffer.getShort();
        byte[] data = parseByteArray(2);
        if (data == null) {
            return null;
        }
        return ControlMessage.createUhidInput(id, data);
    }

    private static Position readPosition(ByteBuffer buffer) {
        int x = buffer.getInt();
        int y = buffer.getInt();
        int screenWidth = Binary.toUnsigned(buffer.getShort());
        int screenHeight = Binary.toUnsigned(buffer.getShort());
        return new Position(x, y, screenWidth, screenHeight);
    }
}
