package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.util.StringUtils;

import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public class DeviceMessageWriter {

    private static final int MESSAGE_MAX_SIZE = 1 << 18; // 256k
    public static final int CLIPBOARD_TEXT_MAX_LENGTH = MESSAGE_MAX_SIZE - 5; // type: 1 byte; length: 4 bytes
    private static final int MEDIA_TEXT_MAX_LENGTH = 1024;

    private final DataOutputStream dos;

    public DeviceMessageWriter(OutputStream rawOutputStream) {
        dos = new DataOutputStream(new BufferedOutputStream(rawOutputStream));
    }

    public void write(DeviceMessage msg) throws IOException {
        int type = msg.getType();
        dos.writeByte(type);
        switch (type) {
            case DeviceMessage.TYPE_CLIPBOARD:
                String text = msg.getText();
                byte[] raw = text.getBytes(StandardCharsets.UTF_8);
                int len = StringUtils.getUtf8TruncationIndex(raw, CLIPBOARD_TEXT_MAX_LENGTH);
                dos.writeInt(len);
                dos.write(raw, 0, len);
                break;
            case DeviceMessage.TYPE_ACK_CLIPBOARD:
                dos.writeLong(msg.getSequence());
                break;
            case DeviceMessage.TYPE_UHID_OUTPUT:
                dos.writeShort(msg.getId());
                byte[] data = msg.getData();
                dos.writeShort(data.length);
                dos.write(data);
                break;
            case DeviceMessage.TYPE_MEDIA_UPDATE:
                dos.writeByte(msg.getState());
                dos.writeLong(msg.getPosition());
                dos.writeInt(msg.getCapabilities());
                dos.writeLong(msg.getDuration());
                writeMediaText(msg.getTitle());
                writeMediaText(msg.getArtist());
                writeMediaText(msg.getAlbum());
                break;
            default:
                throw new ControlProtocolException("Unknown event type: " + type);
        }
        dos.flush();
    }

    // Write a string (2-byte length + UTF-8 content), null is written as an empty string
    private void writeMediaText(String text) throws IOException {
        if (text == null) {
            dos.writeShort(0);
            return;
        }
        byte[] raw = text.getBytes(StandardCharsets.UTF_8);
        int len = StringUtils.getUtf8TruncationIndex(raw, MEDIA_TEXT_MAX_LENGTH);
        dos.writeShort(len);
        dos.write(raw, 0, len);
    }
}
