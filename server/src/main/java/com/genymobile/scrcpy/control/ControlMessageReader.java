package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.device.Position;
import com.genymobile.scrcpy.util.Binary;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

public class ControlMessageReader {

    private static final int MESSAGE_MAX_SIZE = 1 << 18; // 256k

    public static final int CLIPBOARD_TEXT_MAX_LENGTH = MESSAGE_MAX_SIZE - 14; // type: 1 byte; sequence: 8 bytes; paste flag: 1 byte; length: 4 bytes
    public static final int INJECT_TEXT_MAX_LENGTH = 300;

    private final DataInputStream dis;

    public ControlMessageReader(InputStream rawInputStream) {
        dis = new DataInputStream(new BufferedInputStream(rawInputStream));
    }

    public ControlMessage read() throws IOException {
        int type = dis.readUnsignedByte();
        switch (type) {
            case ControlMessage.TYPE_INJECT_KEYCODE:
                return parseInjectKeycode();
            case ControlMessage.TYPE_INJECT_TEXT:
                return parseInjectText();
            case ControlMessage.TYPE_INJECT_TOUCH_EVENT:
                return parseInjectTouchEvent();
            case ControlMessage.TYPE_INJECT_SCROLL_EVENT:
                return parseInjectScrollEvent();
            case ControlMessage.TYPE_BACK_OR_SCREEN_ON:
                return parseBackOrScreenOnEvent();
            case ControlMessage.TYPE_GET_CLIPBOARD:
                return parseGetClipboard();
            case ControlMessage.TYPE_SET_CLIPBOARD:
                return parseSetClipboard();
            case ControlMessage.TYPE_SET_DISPLAY_POWER:
                return parseSetDisplayPower();
            case ControlMessage.TYPE_EXPAND_NOTIFICATION_PANEL:
            case ControlMessage.TYPE_EXPAND_SETTINGS_PANEL:
            case ControlMessage.TYPE_COLLAPSE_PANELS:
            case ControlMessage.TYPE_ROTATE_DEVICE:
            case ControlMessage.TYPE_OPEN_HARD_KEYBOARD_SETTINGS:
            case ControlMessage.TYPE_RESET_VIDEO:
                return ControlMessage.createEmpty(type);
            case ControlMessage.TYPE_UHID_CREATE:
                return parseUhidCreate();
            case ControlMessage.TYPE_UHID_INPUT:
                return parseUhidInput();
            case ControlMessage.TYPE_UHID_DESTROY:
                return parseUhidDestroy();
            case ControlMessage.TYPE_START_APP:
                return parseStartApp();
            default:
                throw new ControlProtocolException("Unknown event type: " + type);
        }
    }

    private ControlMessage parseInjectKeycode() throws IOException {
        int action = dis.readUnsignedByte();
        int keycode = dis.readInt();
        int repeat = dis.readInt();
        int metaState = dis.readInt();
        return ControlMessage.createInjectKeycode(action, keycode, repeat, metaState);
    }

    private int parseBufferLength(int sizeBytes) throws IOException {
        assert sizeBytes > 0 && sizeBytes <= 4;
        int value = 0;
        for (int i = 0; i < sizeBytes; ++i) {
            value = (value << 8) | dis.readUnsignedByte();
        }
        return value;
    }

    private String parseString(int sizeBytes) throws IOException {
        assert sizeBytes > 0 && sizeBytes <= 4;
        byte[] data = parseByteArray(sizeBytes);
        return new String(data, StandardCharsets.UTF_8);
    }

    private String parseString() throws IOException {
        return parseString(4);
    }

    private byte[] parseByteArray(int sizeBytes) throws IOException {
        int len = parseBufferLength(sizeBytes);
        byte[] data = new byte[len];
        dis.readFully(data);
        return data;
    }

    private ControlMessage parseInjectText() throws IOException {
        String text = parseString();
        return ControlMessage.createInjectText(text);
    }

    private ControlMessage parseInjectTouchEvent() throws IOException {
        int action = dis.readUnsignedByte();
        long pointerId = dis.readLong();
        Position position = parsePosition();
        float pressure = Binary.u16FixedPointToFloat(dis.readShort());
        int actionButton = dis.readInt();
        int buttons = dis.readInt();
        return ControlMessage.createInjectTouchEvent(action, pointerId, position, pressure, actionButton, buttons);
    }

    private ControlMessage parseInjectScrollEvent() throws IOException {
        Position position = parsePosition();
        float hScroll = Binary.i16FixedPointToFloat(dis.readShort());
        float vScroll = Binary.i16FixedPointToFloat(dis.readShort());
        int buttons = dis.readInt();
        return ControlMessage.createInjectScrollEvent(position, hScroll, vScroll, buttons);
    }

    private ControlMessage parseBackOrScreenOnEvent() throws IOException {
        int action = dis.readUnsignedByte();
        return ControlMessage.createBackOrScreenOn(action);
    }

    private ControlMessage parseGetClipboard() throws IOException {
        int copyKey = dis.readUnsignedByte();
        return ControlMessage.createGetClipboard(copyKey);
    }

    private ControlMessage parseSetClipboard() throws IOException {
        long sequence = dis.readLong();
        boolean paste = dis.readByte() != 0;
        String text = parseString();
        return ControlMessage.createSetClipboard(sequence, text, paste);
    }

    private ControlMessage parseSetDisplayPower() throws IOException {
        boolean on = dis.readBoolean();
        return ControlMessage.createSetDisplayPower(on);
    }

    private ControlMessage parseUhidCreate() throws IOException {
        int id = dis.readUnsignedShort();
        int vendorId = dis.readUnsignedShort();
        int productId = dis.readUnsignedShort();
        String name = parseString(1);
        byte[] data = parseByteArray(2);
        return ControlMessage.createUhidCreate(id, vendorId, productId, name, data);
    }

    private ControlMessage parseUhidInput() throws IOException {
        int id = dis.readUnsignedShort();
        byte[] data = parseByteArray(2);
        return ControlMessage.createUhidInput(id, data);
    }

    private ControlMessage parseUhidDestroy() throws IOException {
        int id = dis.readUnsignedShort();
        return ControlMessage.createUhidDestroy(id);
    }

    private ControlMessage parseStartApp() throws IOException {
        String name = parseString(1);
        return ControlMessage.createStartApp(name);
    }

    private Position parsePosition() throws IOException {
        int x = dis.readInt();
        int y = dis.readInt();
        int screenWidth = dis.readUnsignedShort();
        int screenHeight = dis.readUnsignedShort();
        return new Position(x, y, screenWidth, screenHeight);
    }
}
