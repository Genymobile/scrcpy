package com.genymobile.scrcpy;

import org.junit.Assert;
import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class DeviceMessageWriterTest {

    @Test
    public void testSerializeClipboard() throws IOException {
        DeviceMessageWriter writer = new DeviceMessageWriter();

        String text = "aéûoç";
        byte[] data = text.getBytes(StandardCharsets.UTF_8);
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(DeviceMessage.TYPE_CLIPBOARD);
        dos.writeShort(data.length);
        dos.write(data);

        byte[] expected = bos.toByteArray();

        DeviceMessage msg = DeviceMessage.createClipboard(text);
        bos = new ByteArrayOutputStream();
        writer.writeTo(msg, bos);

        byte[] actual = bos.toByteArray();

        Assert.assertArrayEquals(expected, actual);
    }
}
