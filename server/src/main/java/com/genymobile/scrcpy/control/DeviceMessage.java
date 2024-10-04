package com.genymobile.scrcpy.control;

public final class DeviceMessage {

    public static final int TYPE_CLIPBOARD = 0;
    public static final int TYPE_ACK_CLIPBOARD = 1;
    public static final int TYPE_UHID_OUTPUT = 2;
    public static final int TYPE_MEDIA_UPDATE = 3;
    public static final int TYPE_MEDIA_REMOVE = 4;
    public static final int TYPE_MEDIA_STATE = 4;

    private int type;
    private String text;
    private long sequence;
    private int id;
    private byte[] data;
    private int mediaState;

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

    public static DeviceMessage createMediaUpdate(int id, byte[] data) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_MEDIA_UPDATE;
        event.id = id;
        event.data = data;
        return event;
    }

    public static DeviceMessage createMediaState(int id, int state) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_MEDIA_STATE;
        event.id = id;
        event.mediaState = state;
        return event;
    }

    public static DeviceMessage createMediaRemove(int id) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_MEDIA_REMOVE;
        event.id = id;
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
}
