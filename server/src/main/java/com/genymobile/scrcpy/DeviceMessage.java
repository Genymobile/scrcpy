package com.genymobile.scrcpy;

public final class DeviceMessage {

    public static final int TYPE_CLIPBOARD = 0;

    private int type;
    private String text;

    private DeviceMessage() {
    }

    public static DeviceMessage createClipboard(String text) {
        DeviceMessage event = new DeviceMessage();
        event.type = TYPE_CLIPBOARD;
        event.text = text;
        return event;
    }

    public int getType() {
        return type;
    }

    public String getText() {
        return text;
    }
}
