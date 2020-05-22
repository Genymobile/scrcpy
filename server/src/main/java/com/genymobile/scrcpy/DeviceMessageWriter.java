package com.genymobile.scrcpy;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class DeviceMessageWriter {

    public static final int CLIPBOARD_TEXT_MAX_LENGTH = 4093;
    private static final int MAX_EVENT_SIZE = CLIPBOARD_TEXT_MAX_LENGTH + 3;

    private final byte[] rawBuffer = new byte[MAX_EVENT_SIZE];
    private final ByteBuffer buffer = ByteBuffer.wrap(rawBuffer);

    public void writeTo(DeviceMessage msg, OutputStream output) throws IOException {
        buffer.clear();
        buffer.put((byte) DeviceMessage.TYPE_CLIPBOARD);
        switch (msg.getType()) {
            case DeviceMessage.TYPE_CLIPBOARD:
                String text = msg.getText();
                byte[] raw = text.getBytes(StandardCharsets.UTF_8);
                int len = StringUtils.getUtf8TruncationIndex(raw, CLIPBOARD_TEXT_MAX_LENGTH);
                buffer.putShort((short) len);
                buffer.put(raw, 0, len);
                output.write(rawBuffer, 0, buffer.position());
                break;
            default:
                Ln.w("Unknown device message: " + msg.getType());
                break;
        }
    }
}
