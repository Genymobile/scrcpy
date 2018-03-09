package com.genymobile.scrcpy;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class ControlEventReader {

    private static final int KEYCODE_PAYLOAD_LENGTH = 9;
    private static final int MOUSE_PAYLOAD_LENGTH = 13;
    private static final int SCROLL_PAYLOAD_LENGTH = 16;
    private static final int COMMAND_PAYLOAD_LENGTH = 1;

    public static final int TEXT_MAX_LENGTH = 300;
    private static final int RAW_BUFFER_SIZE = 1024;

    private final byte[] rawBuffer = new byte[RAW_BUFFER_SIZE];
    private final ByteBuffer buffer = ByteBuffer.wrap(rawBuffer);
    private final byte[] textBuffer = new byte[TEXT_MAX_LENGTH];

    public ControlEventReader() {
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
            throw new EOFException("Event controller socket closed");
        }
        buffer.position(head + r);
        buffer.flip();
    }

    public ControlEvent next() {
        if (!buffer.hasRemaining()) {
            return null;
        }
        int savedPosition = buffer.position();

        int type = buffer.get();
        ControlEvent controlEvent;
        switch (type) {
            case ControlEvent.TYPE_KEYCODE:
                controlEvent = parseKeycodeControlEvent();
                break;
            case ControlEvent.TYPE_TEXT:
                controlEvent = parseTextControlEvent();
                break;
            case ControlEvent.TYPE_MOUSE:
                controlEvent = parseMouseControlEvent();
                break;
            case ControlEvent.TYPE_SCROLL:
                controlEvent = parseScrollControlEvent();
                break;
            case ControlEvent.TYPE_COMMAND:
                controlEvent = parseCommandControlEvent();
                break;
            default:
                Ln.w("Unknown event type: " + type);
                controlEvent = null;
                break;
        }

        if (controlEvent == null) {
            // failure, reset savedPosition
            buffer.position(savedPosition);
        }
        return controlEvent;
    }

    private ControlEvent parseKeycodeControlEvent() {
        if (buffer.remaining() < KEYCODE_PAYLOAD_LENGTH) {
            return null;
        }
        int action = toUnsigned(buffer.get());
        int keycode = buffer.getInt();
        int metaState = buffer.getInt();
        return ControlEvent.createKeycodeControlEvent(action, keycode, metaState);
    }

    private ControlEvent parseTextControlEvent() {
        if (buffer.remaining() < 1) {
            return null;
        }
        int len = toUnsigned(buffer.getShort());
        if (buffer.remaining() < len) {
            return null;
        }
        buffer.get(textBuffer, 0, len);
        String text = new String(textBuffer, 0, len, StandardCharsets.UTF_8);
        return ControlEvent.createTextControlEvent(text);
    }

    private ControlEvent parseMouseControlEvent() {
        if (buffer.remaining() < MOUSE_PAYLOAD_LENGTH) {
            return null;
        }
        int action = toUnsigned(buffer.get());
        int buttons = buffer.getInt();
        Position position = readPosition(buffer);
        return ControlEvent.createMotionControlEvent(action, buttons, position);
    }

    private ControlEvent parseScrollControlEvent() {
        if (buffer.remaining() < SCROLL_PAYLOAD_LENGTH) {
            return null;
        }
        Position position = readPosition(buffer);
        int hScroll = buffer.getInt();
        int vScroll = buffer.getInt();
        return ControlEvent.createScrollControlEvent(position, hScroll, vScroll);
    }

    private ControlEvent parseCommandControlEvent() {
        if (buffer.remaining() < COMMAND_PAYLOAD_LENGTH) {
            return null;
        }
        int action = toUnsigned(buffer.get());
        return ControlEvent.createCommandControlEvent(action);
    }

    private static Position readPosition(ByteBuffer buffer) {
        int x = toUnsigned(buffer.getShort());
        int y = toUnsigned(buffer.getShort());
        int screenWidth = toUnsigned(buffer.getShort());
        int screenHeight = toUnsigned(buffer.getShort());
        return new Position(x, y, screenWidth, screenHeight);
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static int toUnsigned(short value) {
        return value & 0xffff;
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static int toUnsigned(byte value) {
        return value & 0xff;
    }
}
