package com.genymobile.scrcpy;

import java.io.IOException;

public final class DeviceMessageSender {

    private final DesktopConnection connection;

    private String clipboardText;

    private long ack;

    public DeviceMessageSender(DesktopConnection connection) {
        this.connection = connection;
    }

    public synchronized void pushClipboardText(String text) {
        clipboardText = text;
        notify();
    }

    public synchronized void pushAckClipboard(long sequence) {
        ack = sequence;
        notify();
    }

    public void loop() throws IOException, InterruptedException {
        while (true) {
            String text;
            long sequence;
            synchronized (this) {
                while (ack == DeviceMessage.SEQUENCE_INVALID && clipboardText == null) {
                    wait();
                }
                text = clipboardText;
                clipboardText = null;

                sequence = ack;
                ack = DeviceMessage.SEQUENCE_INVALID;
            }

            if (sequence != DeviceMessage.SEQUENCE_INVALID) {
                DeviceMessage event = DeviceMessage.createAckClipboard(sequence);
                connection.sendDeviceMessage(event);
            }
            if (text != null) {
                DeviceMessage event = DeviceMessage.createClipboard(text);
                connection.sendDeviceMessage(event);
            }
        }
    }
}
