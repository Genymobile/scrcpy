package com.genymobile.scrcpy;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class ControlEventReader {

    private static final int KEYCODE_PAYLOAD_LENGTH = 9;
    private static final int MOUSE_PAYLOAD_LENGTH = 13;
    private static final int SCROLL_PAYLOAD_LENGTH = 16;
    private static final int COMMAND_PAYLOAD_LENGTH = 1;

    private final byte[] rawBuffer = new byte[128];
    private final ByteBuffer buffer = ByteBuffer.wrap(rawBuffer);
    private final byte[] textBuffer = new byte[32];

    public ControlEventReader() {
        // invariant: the buffer is always in "get" mode
        buffer.limit(0);
    }

    public boolean isFull() {
        return buffer.remaining() == rawBuffer.length;
    }

    public boolean readFrom(InputStream input) throws IOException {
        if (isFull()) {
            throw new IllegalStateException("Buffer full, call next() to consume");
        }
        buffer.compact();
        int head = buffer.position();
        int r = input.read(rawBuffer, head, rawBuffer.length - head);
        if (r == -1) {
            return false;
        }
        buffer.position(head + r);
        buffer.flip();
        return true;
    }

    public ControlEvent next() {
        if (!buffer.hasRemaining()) {
            return null;
        }
        int savedPosition = buffer.position();

        int type = buffer.get();
        switch (type) {
            case ControlEvent.TYPE_KEYCODE: {
                if (buffer.remaining() < KEYCODE_PAYLOAD_LENGTH) {
                    break;
                }
                int action = toUnsigned(buffer.get());
                int keycode = buffer.getInt();
                int metaState = buffer.getInt();
                return ControlEvent.createKeycodeControlEvent(action, keycode, metaState);
            }
            case ControlEvent.TYPE_TEXT: {
                if (buffer.remaining() < 1) {
                    break;
                }
                int len = toUnsigned(buffer.get());
                if (buffer.remaining() < len) {
                    break;
                }
                buffer.get(textBuffer, 0, len);
                String text = new String(textBuffer, 0, len, StandardCharsets.UTF_8);
                return ControlEvent.createTextControlEvent(text);
            }
            case ControlEvent.TYPE_MOUSE: {
                if (buffer.remaining() < MOUSE_PAYLOAD_LENGTH) {
                    break;
                }
                int action = toUnsigned(buffer.get());
                int buttons = buffer.getInt();
                Position position = readPosition(buffer);
                return ControlEvent.createMotionControlEvent(action, buttons, position);
            }
            case ControlEvent.TYPE_SCROLL: {
                if (buffer.remaining() < SCROLL_PAYLOAD_LENGTH) {
                    break;
                }
                Position position = readPosition(buffer);
                int hScroll = buffer.getInt();
                int vScroll = buffer.getInt();
                return ControlEvent.createScrollControlEvent(position, hScroll, vScroll);
            }
            case ControlEvent.TYPE_COMMAND: {
                if (buffer.remaining() < COMMAND_PAYLOAD_LENGTH) {
                    break;
                }
                int action = toUnsigned(buffer.get());
                return ControlEvent.createCommandControlEvent(action);
            }
            default:
                Ln.w("Unknown event type: " + type);
        }

        // failure, reset savedPosition
        buffer.position(savedPosition);
        return null;
    }

    private static Position readPosition(ByteBuffer buffer) {
        int x = toUnsigned(buffer.getShort());
        int y = toUnsigned(buffer.getShort());
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
