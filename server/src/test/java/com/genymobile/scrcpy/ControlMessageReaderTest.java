package com.genymobile.scrcpy;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.junit.Assert;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;


public class ControlMessageReaderTest {

    @Test
    public void testParseKeycodeEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(KeyEvent.META_CTRL_ON);
        byte[] packet = bos.toByteArray();

        // The message type (1 byte) does not count
        Assert.assertEquals(ControlMessageReader.INJECT_KEYCODE_PAYLOAD_LENGTH, packet.length - 1);

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());
    }

    @Test
    public void testParseTextEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_TEXT);
        byte[] text = "testé".getBytes(StandardCharsets.UTF_8);
        dos.writeShort(text.length);
        dos.write(text);
        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_INJECT_TEXT, event.getType());
        Assert.assertEquals("testé", event.getText());
    }

    @Test
    public void testParseLongTextEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_TEXT);
        byte[] text = new byte[ControlMessageReader.INJECT_TEXT_MAX_LENGTH];
        Arrays.fill(text, (byte) 'a');
        dos.writeShort(text.length);
        dos.write(text);
        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_INJECT_TEXT, event.getType());
        Assert.assertEquals(new String(text, StandardCharsets.US_ASCII), event.getText());
    }

    @Test
    public void testParseTouchEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_TOUCH_EVENT);
        dos.writeByte(MotionEvent.ACTION_DOWN);
        dos.writeLong(-42); // pointerId
        dos.writeInt(100);
        dos.writeInt(200);
        dos.writeShort(1080);
        dos.writeShort(1920);
        dos.writeShort(0xffff); // pressure
        dos.writeInt(MotionEvent.BUTTON_PRIMARY);

        byte[] packet = bos.toByteArray();

        // The message type (1 byte) does not count
        Assert.assertEquals(ControlMessageReader.INJECT_TOUCH_EVENT_PAYLOAD_LENGTH, packet.length - 1);

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_INJECT_TOUCH_EVENT, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(-42, event.getPointerId());
        Assert.assertEquals(100, event.getPosition().getPoint().getX());
        Assert.assertEquals(200, event.getPosition().getPoint().getY());
        Assert.assertEquals(1080, event.getPosition().getScreenSize().getWidth());
        Assert.assertEquals(1920, event.getPosition().getScreenSize().getHeight());
        Assert.assertEquals(1f, event.getPressure(), 0f); // must be exact
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getButtons());
    }

    @Test
    public void testParseScrollEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_SCROLL_EVENT);
        dos.writeInt(260);
        dos.writeInt(1026);
        dos.writeShort(1080);
        dos.writeShort(1920);
        dos.writeInt(1);
        dos.writeInt(-1);

        byte[] packet = bos.toByteArray();

        // The message type (1 byte) does not count
        Assert.assertEquals(ControlMessageReader.INJECT_SCROLL_EVENT_PAYLOAD_LENGTH, packet.length - 1);

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_INJECT_SCROLL_EVENT, event.getType());
        Assert.assertEquals(260, event.getPosition().getPoint().getX());
        Assert.assertEquals(1026, event.getPosition().getPoint().getY());
        Assert.assertEquals(1080, event.getPosition().getScreenSize().getWidth());
        Assert.assertEquals(1920, event.getPosition().getScreenSize().getHeight());
        Assert.assertEquals(1, event.getHScroll());
        Assert.assertEquals(-1, event.getVScroll());
    }

    @Test
    public void testParseBackOrScreenOnEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_BACK_OR_SCREEN_ON);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_BACK_OR_SCREEN_ON, event.getType());
    }

    @Test
    public void testParseExpandNotificationPanelEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL, event.getType());
    }

    @Test
    public void testParseCollapseNotificationPanelEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_COLLAPSE_NOTIFICATION_PANEL);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_COLLAPSE_NOTIFICATION_PANEL, event.getType());
    }

    @Test
    public void testParseGetClipboardEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_GET_CLIPBOARD);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_GET_CLIPBOARD, event.getType());
    }

    @Test
    public void testParseSetClipboardEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_SET_CLIPBOARD);
        dos.writeByte(1); // paste
        byte[] text = "testé".getBytes(StandardCharsets.UTF_8);
        dos.writeShort(text.length);
        dos.write(text);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_SET_CLIPBOARD, event.getType());
        Assert.assertEquals("testé", event.getText());

        boolean parse = (event.getFlags() & ControlMessage.FLAGS_PASTE) != 0;
        Assert.assertTrue(parse);
    }

    @Test
    public void testParseBigSetClipboardEvent() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_SET_CLIPBOARD);

        byte[] rawText = new byte[ControlMessageReader.CLIPBOARD_TEXT_MAX_LENGTH];
        dos.writeByte(1); // paste
        Arrays.fill(rawText, (byte) 'a');
        String text = new String(rawText, 0, rawText.length);

        dos.writeShort(rawText.length);
        dos.write(rawText);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_SET_CLIPBOARD, event.getType());
        Assert.assertEquals(text, event.getText());

        boolean parse = (event.getFlags() & ControlMessage.FLAGS_PASTE) != 0;
        Assert.assertTrue(parse);
    }

    @Test
    public void testParseSetScreenPowerMode() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_SET_SCREEN_POWER_MODE);
        dos.writeByte(Device.POWER_MODE_NORMAL);

        byte[] packet = bos.toByteArray();

        // The message type (1 byte) does not count
        Assert.assertEquals(ControlMessageReader.SET_SCREEN_POWER_MODE_PAYLOAD_LENGTH, packet.length - 1);

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_SET_SCREEN_POWER_MODE, event.getType());
        Assert.assertEquals(Device.POWER_MODE_NORMAL, event.getAction());
    }

    @Test
    public void testParseRotateDevice() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_ROTATE_DEVICE);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlMessage event = reader.next();

        Assert.assertEquals(ControlMessage.TYPE_ROTATE_DEVICE, event.getType());
    }

    @Test
    public void testMultiEvents() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(KeyEvent.META_CTRL_ON);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(MotionEvent.ACTION_DOWN);
        dos.writeInt(MotionEvent.BUTTON_PRIMARY);
        dos.writeInt(KeyEvent.META_CTRL_ON);

        byte[] packet = bos.toByteArray();
        reader.readFrom(new ByteArrayInputStream(packet));

        ControlMessage event = reader.next();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());

        event = reader.next();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());
    }

    @Test
    public void testPartialEvents() throws IOException {
        ControlMessageReader reader = new ControlMessageReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(KeyEvent.META_CTRL_ON);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(MotionEvent.ACTION_DOWN);

        byte[] packet = bos.toByteArray();
        reader.readFrom(new ByteArrayInputStream(packet));

        ControlMessage event = reader.next();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());

        event = reader.next();
        Assert.assertNull(event); // the event is not complete

        bos.reset();
        dos.writeInt(MotionEvent.BUTTON_PRIMARY);
        dos.writeInt(KeyEvent.META_CTRL_ON);
        packet = bos.toByteArray();
        reader.readFrom(new ByteArrayInputStream(packet));

        // the event is now complete
        event = reader.next();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());
    }
}
