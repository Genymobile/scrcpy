package com.genymobile.scrcpy.ime;

import org.junit.Assert;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class ImeProtocolTest {
    @Test
    public void testHandshake() throws IOException {
        String ime = "com.example/.InputMethod";
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        ImeProtocol.writeHandshake(new DataOutputStream(bytes), ime);

        DataInputStream input = new DataInputStream(new ByteArrayInputStream(bytes.toByteArray()));
        Assert.assertEquals(ImeProtocol.MAGIC, input.readInt());
        Assert.assertEquals(ImeProtocol.VERSION, input.readUnsignedByte());
        byte[] imeBytes = new byte[input.readUnsignedShort()];
        input.readFully(imeBytes);
        Assert.assertEquals(ime, new String(imeBytes, StandardCharsets.UTF_8));
        Assert.assertEquals(-1, input.read());
    }

    @Test
    public void testUnicodeText() throws IOException {
        String text = "中文🙂 café";
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        ImeProtocol.writeText(new DataOutputStream(bytes), text);

        DataInputStream input = new DataInputStream(new ByteArrayInputStream(bytes.toByteArray()));
        Assert.assertEquals(ImeProtocol.FRAME_TEXT, input.readUnsignedByte());
        byte[] textBytes = new byte[input.readInt()];
        input.readFully(textBytes);
        Assert.assertEquals(text, new String(textBytes, StandardCharsets.UTF_8));
    }

    @Test
    public void testTextLengthBoundary() throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        ImeProtocol.writeText(new DataOutputStream(bytes), repeat("🙂", 75));

        try {
            ImeProtocol.writeText(new DataOutputStream(new ByteArrayOutputStream()), repeat("🙂", 76));
            Assert.fail("Expected payload length failure");
        } catch (IOException e) {
            Assert.assertTrue(e.getMessage().contains("300"));
        }
    }

    private static String repeat(String value, int count) {
        StringBuilder builder = new StringBuilder(value.length() * count);
        for (int i = 0; i < count; ++i) {
            builder.append(value);
        }
        return builder.toString();
    }
}
