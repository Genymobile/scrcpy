package com.genymobile.scrcpy;

public final class DeviceEvent {

    public static final int TYPE_GET_CLIPBOARD = 0;

    private int type;
    private String text;

    private DeviceEvent() {
    }

    public static DeviceEvent createGetClipboardEvent(String text) {
        DeviceEvent event = new DeviceEvent();
        event.type = TYPE_GET_CLIPBOARD;
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
