package com.genymobile.scrcpy;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class DeviceMessageWriter {

    private static final int MESSAGE_MAX_SIZE = 1 << 18; // 256k
    public static final int CLIPBOARD_TEXT_MAX_LENGTH = MESSAGE_MAX_SIZE - 5; // type: 1 byte; length: 4 bytes

    private final byte[] rawBuffer = new byte[MESSAGE_MAX_SIZE];
    private final ByteBuffer buffer = ByteBuffer.wrap(rawBuffer);

    public void writeTo(DeviceMessage msg, OutputStream output) throws IOException {
        buffer.clear();
        buffer.put((byte) msg.getType());
        switch (msg.getType()) {
            case DeviceMessage.TYPE_CLIPBOARD:
                String text = msg.getText();
                byte[] raw = text.getBytes(StandardCharsets.UTF_8);
                int len = StringUtils.getUtf8TruncationIndex(raw, CLIPBOARD_TEXT_MAX_LENGTH);
                buffer.putInt(len);
                buffer.put(raw, 0, len);
                output.write(rawBuffer, 0, buffer.position());
                break;
            case DeviceMessage.TYPE_ACK_CLIPBOARD:
                buffer.putLong(msg.getSequence());
                output.write(rawBuffer, 0, buffer.position());
                break;
            case DeviceMessage.TYPE_UHID_OUTPUT:
                buffer.putShort((short) msg.getId());
                byte[] data = msg.getData();
                buffer.putShort((short) data.length);
                buffer.put(data);
                output.write(rawBuffer, 0, buffer.position());
                break;
            default:
                Ln.w("Unknown device message: " + msg.getType());
                break;
        }
    }
}
