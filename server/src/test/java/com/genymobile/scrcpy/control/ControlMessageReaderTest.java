package com.genymobile.scrcpy.control;

import android.view.KeyEvent;
import android.view.MotionEvent;
import org.junit.Assert;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class ControlMessageReaderTest {

    @Test
    public void testParseKeycodeEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(5); // repeat
        dos.writeInt(KeyEvent.META_CTRL_ON);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(5, event.getRepeat());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseTextEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_TEXT);
        byte[] text = "testé".getBytes(StandardCharsets.UTF_8);
        dos.writeInt(text.length);
        dos.write(text);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_TEXT, event.getType());
        Assert.assertEquals("testé", event.getText());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseLongTextEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_TEXT);
        byte[] text = new byte[ControlMessageReader.INJECT_TEXT_MAX_LENGTH];
        Arrays.fill(text, (byte) 'a');
        dos.writeInt(text.length);
        dos.write(text);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_TEXT, event.getType());
        Assert.assertEquals(new String(text, StandardCharsets.US_ASCII), event.getText());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseTouchEvent() throws IOException {
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
        dos.writeInt(MotionEvent.BUTTON_PRIMARY); // action button
        dos.writeInt(MotionEvent.BUTTON_PRIMARY); // buttons

        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_TOUCH_EVENT, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(-42, event.getPointerId());
        Assert.assertEquals(100, event.getPosition().getPoint().getX());
        Assert.assertEquals(200, event.getPosition().getPoint().getY());
        Assert.assertEquals(1080, event.getPosition().getScreenSize().getWidth());
        Assert.assertEquals(1920, event.getPosition().getScreenSize().getHeight());
        Assert.assertEquals(1f, event.getPressure(), 0f); // must be exact
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getActionButton());
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getButtons());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseScrollEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_INJECT_SCROLL_EVENT);
        dos.writeInt(260);
        dos.writeInt(1026);
        dos.writeShort(1080);
        dos.writeShort(1920);
        dos.writeShort(0); // 0.0f encoded as i16
        dos.writeShort(0x8000); // -16.0f encoded as i16 (the range is [-16, 16])
        dos.writeInt(1);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_SCROLL_EVENT, event.getType());
        Assert.assertEquals(260, event.getPosition().getPoint().getX());
        Assert.assertEquals(1026, event.getPosition().getPoint().getY());
        Assert.assertEquals(1080, event.getPosition().getScreenSize().getWidth());
        Assert.assertEquals(1920, event.getPosition().getScreenSize().getHeight());
        Assert.assertEquals(0f, event.getHScroll(), 0f);
        Assert.assertEquals(-16f, event.getVScroll(), 0f);
        Assert.assertEquals(1, event.getButtons());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseBackOrScreenOnEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_BACK_OR_SCREEN_ON);
        dos.writeByte(KeyEvent.ACTION_UP);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_BACK_OR_SCREEN_ON, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseExpandNotificationPanelEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL, event.getType());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseExpandSettingsPanelEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_EXPAND_SETTINGS_PANEL);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_EXPAND_SETTINGS_PANEL, event.getType());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseCollapsePanelsEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_COLLAPSE_PANELS);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_COLLAPSE_PANELS, event.getType());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseGetClipboardEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_GET_CLIPBOARD);
        dos.writeByte(ControlMessage.COPY_KEY_COPY);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_GET_CLIPBOARD, event.getType());
        Assert.assertEquals(ControlMessage.COPY_KEY_COPY, event.getCopyKey());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseSetClipboardEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_SET_CLIPBOARD);
        dos.writeLong(0x0102030405060708L); // sequence
        dos.writeByte(1); // paste
        byte[] text = "testé".getBytes(StandardCharsets.UTF_8);
        dos.writeInt(text.length);
        dos.write(text);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_SET_CLIPBOARD, event.getType());
        Assert.assertEquals(0x0102030405060708L, event.getSequence());
        Assert.assertEquals("testé", event.getText());
        Assert.assertTrue(event.getPaste());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseBigSetClipboardEvent() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_SET_CLIPBOARD);

        byte[] rawText = new byte[ControlMessageReader.CLIPBOARD_TEXT_MAX_LENGTH];
        dos.writeLong(0x0807060504030201L); // sequence
        dos.writeByte(1); // paste
        Arrays.fill(rawText, (byte) 'a');
        String text = new String(rawText, 0, rawText.length);

        dos.writeInt(rawText.length);
        dos.write(rawText);

        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_SET_CLIPBOARD, event.getType());
        Assert.assertEquals(0x0807060504030201L, event.getSequence());
        Assert.assertEquals(text, event.getText());
        Assert.assertTrue(event.getPaste());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseSetDisplayPower() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_SET_DISPLAY_POWER);
        dos.writeBoolean(true);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_SET_DISPLAY_POWER, event.getType());
        Assert.assertTrue(event.getOn());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseRotateDevice() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_ROTATE_DEVICE);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_ROTATE_DEVICE, event.getType());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseUhidCreate() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_UHID_CREATE);
        dos.writeShort(42); // id
        dos.writeShort(0x1234); // vendorId
        dos.writeShort(0x5678); // productId
        dos.writeByte(3); // name size
        dos.write("ABC".getBytes(StandardCharsets.US_ASCII));
        byte[] data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
        dos.writeShort(data.length); // report desc size
        dos.write(data);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_UHID_CREATE, event.getType());
        Assert.assertEquals(42, event.getId());
        Assert.assertEquals(0x1234, event.getVendorId());
        Assert.assertEquals(0x5678, event.getProductId());
        Assert.assertEquals("ABC", event.getText());
        Assert.assertArrayEquals(data, event.getData());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseUhidInput() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_UHID_INPUT);
        dos.writeShort(42); // id
        byte[] data = {1, 2, 3, 4, 5};
        dos.writeShort(data.length); // size
        dos.write(data);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_UHID_INPUT, event.getType());
        Assert.assertEquals(42, event.getId());
        Assert.assertArrayEquals(data, event.getData());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseUhidDestroy() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_UHID_DESTROY);
        dos.writeShort(42); // id
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_UHID_DESTROY, event.getType());
        Assert.assertEquals(42, event.getId());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseOpenHardKeyboardSettings() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_OPEN_HARD_KEYBOARD_SETTINGS);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_OPEN_HARD_KEYBOARD_SETTINGS, event.getType());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testParseStartApp() throws IOException {
        byte[] name = "firefox".getBytes(StandardCharsets.UTF_8);

        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);
        dos.writeByte(ControlMessage.TYPE_START_APP);
        dos.writeByte(name.length);
        dos.write(name);
        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_START_APP, event.getType());
        Assert.assertEquals("firefox", event.getText());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testMultiEvents() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(0); // repeat
        dos.writeInt(KeyEvent.META_CTRL_ON);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(MotionEvent.ACTION_DOWN);
        dos.writeInt(MotionEvent.BUTTON_PRIMARY);
        dos.writeInt(1); // repeat
        dos.writeInt(KeyEvent.META_CTRL_ON);

        byte[] packet = bos.toByteArray();

        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(0, event.getRepeat());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());

        event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(MotionEvent.ACTION_DOWN, event.getAction());
        Assert.assertEquals(MotionEvent.BUTTON_PRIMARY, event.getKeycode());
        Assert.assertEquals(1, event.getRepeat());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());

        Assert.assertEquals(-1, bis.read()); // EOS
    }

    @Test
    public void testPartialEvents() throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(bos);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(KeyEvent.ACTION_UP);
        dos.writeInt(KeyEvent.KEYCODE_ENTER);
        dos.writeInt(4); // repeat
        dos.writeInt(KeyEvent.META_CTRL_ON);

        dos.writeByte(ControlMessage.TYPE_INJECT_KEYCODE);
        dos.writeByte(MotionEvent.ACTION_DOWN);

        byte[] packet = bos.toByteArray();
        ByteArrayInputStream bis = new ByteArrayInputStream(packet);
        ControlMessageReader reader = new ControlMessageReader(bis);

        ControlMessage event = reader.read();
        Assert.assertEquals(ControlMessage.TYPE_INJECT_KEYCODE, event.getType());
        Assert.assertEquals(KeyEvent.ACTION_UP, event.getAction());
        Assert.assertEquals(KeyEvent.KEYCODE_ENTER, event.getKeycode());
        Assert.assertEquals(4, event.getRepeat());
        Assert.assertEquals(KeyEvent.META_CTRL_ON, event.getMetaState());

        try {
            event = reader.read();
            Assert.fail("Reader did not reach EOF");
        } catch (EOFException e) {
            // expected
        }
    }
}
