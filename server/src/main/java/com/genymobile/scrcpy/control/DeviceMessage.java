package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.model.Point;
import com.genymobile.scrcpy.model.Size;

public final class DeviceMessage {

    public static final int TYPE_CLIPBOARD = 0;
    public static final int TYPE_ACK_CLIPBOARD = 1;
    public static final int TYPE_UHID_OUTPUT = 2;
    public static final int TYPE_IME_CURSOR_ANCHOR = 3;

    private int type;
    private String text;
    private long sequence;
    private int id;
    private byte[] data;
    private boolean valid;
    private Point anchorStart;
    private Point anchorEnd;
    private Size screenSize;

    private DeviceMessage() {
    }

    public static DeviceMessage createClipboard(String text) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_CLIPBOARD;
        event.text = text;
        return event;
    }

    public static DeviceMessage createAckClipboard(long sequence) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_ACK_CLIPBOARD;
        event.sequence = sequence;
        return event;
    }

    public static DeviceMessage createUhidOutput(int id, byte[] data) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_UHID_OUTPUT;
        event.id = id;
        event.data = data;
        return event;
    }

    public static DeviceMessage createImeCursorAnchor(boolean valid, Point start, Point end, Size screenSize) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_IME_CURSOR_ANCHOR;
        event.valid = valid;
        event.anchorStart = start;
        event.anchorEnd = end;
        event.screenSize = screenSize;
        return event;
    }

    public int getType() {
        return type;
    }

    public String getText() {
        return text;
    }

    public long getSequence() {
        return sequence;
    }

    public int getId() {
        return id;
    }

    public byte[] getData() {
        return data;
    }

    public boolean isValid() {
        return valid;
    }

    public Point getAnchorStart() {
        return anchorStart;
    }

    public Point getAnchorEnd() {
        return anchorEnd;
    }

    public Size getScreenSize() {
        return screenSize;
    }
}
