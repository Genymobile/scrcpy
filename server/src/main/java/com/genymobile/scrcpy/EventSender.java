package com.genymobile.scrcpy;

import java.io.IOException;

public final class EventSender {

    private final DesktopConnection connection;

    private String clipboardText;

    public EventSender(DesktopConnection connection) {
        this.connection = connection;
    }

    public synchronized void pushClipboardText(String text) {
        clipboardText = text;
        notify();
    }

    public void loop() throws IOException, InterruptedException {
        while (true) {
            String text;
            synchronized (this) {
                while (clipboardText == null) {
                    wait();
                }
                text = clipboardText;
                clipboardText = null;
            }
            DeviceEvent event = DeviceEvent.createGetClipboardEvent(text);
            connection.sendDeviceEvent(event);
        }
    }
}
