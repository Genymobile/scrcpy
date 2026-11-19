package com.genymobile.scrcpy.control;

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
    public void testSerializeMediaUpdate() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(DeviceMessage.TYPE_MEDIA_UPDATE);
        dos.writeByte(1); // state (playing)
        dos.writeLong(12345); // position
        dos.writeInt(0x13); // capabilities
        dos.writeLong(123456); // duration
        byte[] title = "hello".getBytes(StandardCharsets.UTF_8);
        dos.writeShort(title.length);
        dos.write(title);
        dos.writeShort(0); // artist (null)
        byte[] album = "foo".getBytes(StandardCharsets.UTF_8);
        dos.writeShort(album.length);
        dos.write(album);
        byte[] expected = bos.toByteArray();

        bos = new ByteArrayOutputStream();
        DeviceMessageWriter writer = new DeviceMessageWriter(bos);

        DeviceMessage msg = DeviceMessage.createMediaUpdate(1, 12345, 0x13, 123456, "hello", null, "foo");
        writer.write(msg);

        byte[] actual = bos.toByteArray();

        Assert.assertArrayEquals(expected, actual);
    }
}
