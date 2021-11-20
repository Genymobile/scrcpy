package com.genymobile.scrcpy;

public final class DeviceMessage {

    public static final int TYPE_CLIPBOARD = 0;
    public static final int TYPE_ACK_CLIPBOARD = 1;

    public static final long SEQUENCE_INVALID = ControlMessage.SEQUENCE_INVALID;

    private int type;
    private String text;
    private long sequence;

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

    public int getType() {
        return type;
    }

    public String getText() {
        return text;
    }

    public long getSequence() {
        return sequence;
    }
}
