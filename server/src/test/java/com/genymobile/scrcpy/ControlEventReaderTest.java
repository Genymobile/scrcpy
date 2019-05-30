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


public class ControlEventReaderTest {

    @Test
    public void testParseKeycodeEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(KeyEvent.META_CTRL_ON);
        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());
    }

    @Test
    public void testParseTextEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_TEXT);
        byte[] text = "testé".getBytes(StandardCharsets.UTF_8);
        dos.writeShort(text.length);
        dos.write(text);
        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_TEXT, event.getType());
        Assert.assertEquals("testé", event.getText());
    }

    @Test
    public void testParseLongTextEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_TEXT);
        byte[] text = new byte[ControlEventReader.TEXT_MAX_LENGTH];
        Arrays.fill(text, (byte) 'a');
        dos.writeShort(text.length);
        dos.write(text);
        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_TEXT, event.getType());
        Assert.assertEquals(new String(text, StandardCharsets.US_ASCII), event.getText());
    }

    @Test
    public void testParseMouseEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_KEYCODE);
        dos.writeByte(MotionEvent.ACTION_DOWN);
        dos.writeInt(MotionEvent.BUTTON_PRIMARY);
        dos.writeInt(KeyEvent.META_CTRL_ON);
        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_KEYCODE, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());
    }

    @Test
    @SuppressWarnings("checkstyle:MagicNumber")
    public void testParseScrollEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_SCROLL);
        dos.writeInt(260);
        dos.writeInt(1026);
        dos.writeShort(1080);
        dos.writeShort(1920);
        dos.writeInt(1);
        dos.writeInt(-1);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_SCROLL, event.getType());
        Assert.assertEquals(260, event.getPosition().getPoint().getX());
        Assert.assertEquals(1026, event.getPosition().getPoint().getY());
        Assert.assertEquals(1080, event.getPosition().getScreenSize().getWidth());
        Assert.assertEquals(1920, event.getPosition().getScreenSize().getHeight());
        Assert.assertEquals(1, event.getHScroll());
        Assert.assertEquals(-1, event.getVScroll());
    }

    @Test
    public void testParseBackOrScreenOnEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_BACK_OR_SCREEN_ON);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_BACK_OR_SCREEN_ON, event.getType());
    }

    @Test
    public void testParseExpandNotificationPanelEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_EXPAND_NOTIFICATION_PANEL);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_EXPAND_NOTIFICATION_PANEL, event.getType());
    }

    @Test
    public void testParseCollapseNotificationPanelEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_COLLAPSE_NOTIFICATION_PANEL);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_COLLAPSE_NOTIFICATION_PANEL, event.getType());
    }

    @Test
    public void testParseGetClipboardEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_GET_CLIPBOARD);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_GET_CLIPBOARD, event.getType());
    }

    @Test
    public void testParseSetClipboardEvent() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlEvent.TYPE_SET_CLIPBOARD);
        byte[] text = "testé".getBytes(StandardCharsets.UTF_8);
        dos.writeShort(text.length);
        dos.write(text);

        byte[] packet = bos.toByteArray();

        reader.readFrom(new ByteArrayInputStream(packet));
        ControlEvent event = reader.next();

        Assert.assertEquals(ControlEvent.TYPE_SET_CLIPBOARD, event.getType());
        Assert.assertEquals("testé", event.getText());
    }

    @Test
    public void testMultiEvents() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);

        dos.writeByte(ControlEvent.TYPE_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(KeyEvent.META_CTRL_ON);

        dos.writeByte(ControlEvent.TYPE_KEYCODE);
        dos.writeByte(MotionEvent.ACTION_DOWN);
        dos.writeInt(MotionEvent.BUTTON_PRIMARY);
        dos.writeInt(KeyEvent.META_CTRL_ON);

        byte[] packet = bos.toByteArray();
        reader.readFrom(new ByteArrayInputStream(packet));

        ControlEvent event = reader.next();
        Assert.assertEquals(ControlEvent.TYPE_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());

        event = reader.next();
        Assert.assertEquals(ControlEvent.TYPE_KEYCODE, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());
    }

    @Test
    public void testPartialEvents() throws IOException {
        ControlEventReader reader = new ControlEventReader();

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);

        dos.writeByte(ControlEvent.TYPE_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(KeyEvent.META_CTRL_ON);

        dos.writeByte(ControlEvent.TYPE_KEYCODE);
        dos.writeByte(MotionEvent.ACTION_DOWN);

        byte[] packet = bos.toByteArray();
        reader.readFrom(new ByteArrayInputStream(packet));

        ControlEvent event = reader.next();
        Assert.assertEquals(ControlEvent.TYPE_KEYCODE, event.getType());
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
        Assert.assertEquals(ControlEvent.TYPE_KEYCODE, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getKeycode());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());
    }
}
