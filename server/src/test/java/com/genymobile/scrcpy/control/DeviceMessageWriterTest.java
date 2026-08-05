package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.model.Point;
import com.genymobile.scrcpy.model.Size;

import org.junit.Assert;
import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class DeviceMessageWriterTest {

    @Test
    public void testSerializeClipboard() throws IOException {
        String text = "aéûoç";
        byte[] data = text.getBytes(StandardCharsets.UTF_8);
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(DeviceMessage.TYPE_CLIPBOARD);
        dos.writeInt(data.length);
        dos.write(data);
        byte[] expected = bos.toByteArray();

        bos = new ByteArrayOutputStream();
        DeviceMessageWriter writer = new DeviceMessageWriter(bos);

        DeviceMessage msg = DeviceMessage.createClipboard(text);
        writer.write(msg);

        byte[] actual = bos.toByteArray();

        Assert.assertArrayEquals(expected, actual);
    }

    @Test
    public void testSerializeAckSetClipboard() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(DeviceMessage.TYPE_ACK_CLIPBOARD);
        dos.writeLong(0x0102030405060708L);
        byte[] expected = bos.toByteArray();

        bos = new ByteArrayOutputStream();
        DeviceMessageWriter writer = new DeviceMessageWriter(bos);

        DeviceMessage msg = DeviceMessage.createAckClipboard(0x0102030405060708L);
        writer.write(msg);

        byte[] actual = bos.toByteArray();

        Assert.assertArrayEquals(expected, actual);
    }

    @Test
    public void testSerializeUhidOutput() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(DeviceMessage.TYPE_UHID_OUTPUT);
        dos.writeShort(42); // id
        byte[] data = {1, 2, 3, 4, 5};
        dos.writeShort(data.length);
        dos.write(data);
        byte[] expected = bos.toByteArray();

        bos = new ByteArrayOutputStream();
        DeviceMessageWriter writer = new DeviceMessageWriter(bos);

        DeviceMessage msg = DeviceMessage.createUhidOutput(42, data);
        writer.write(msg);

        byte[] actual = bos.toByteArray();

        Assert.assertArrayEquals(expected, actual);
    }

    @Test
    public void testSerializeImeCursorAnchor() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(DeviceMessage.TYPE_IME_CURSOR_ANCHOR);
        dos.writeBoolean(true);
        dos.writeInt(100);
        dos.writeInt(200);
        dos.writeInt(100);
        dos.writeInt(220);
        dos.writeShort(1080);
        dos.writeShort(2400);
        byte[] expected = bos.toByteArray();

        bos = new ByteArrayOutputStream();
        DeviceMessageWriter writer = new DeviceMessageWriter(bos);
        DeviceMessage msg = DeviceMessage.createImeCursorAnchor(
                true, new Point(100, 200), new Point(100, 220), new Size(1080, 2400));
        writer.write(msg);

        Assert.assertArrayEquals(expected, bos.toByteArray());
    }
}
