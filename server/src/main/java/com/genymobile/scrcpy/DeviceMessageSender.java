package com.genymobile.scrcpy;

import java.io.IOException;

public final class DeviceMessageSender {

    private final DesktopConnection connection;

    private String clipboardText;

    public DeviceMessageSender(DesktopConnection connection) {
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
            DeviceMessage event = DeviceMessage.createClipboard(text);
            connection.sendDeviceMessage(event);
        }
    }
}
