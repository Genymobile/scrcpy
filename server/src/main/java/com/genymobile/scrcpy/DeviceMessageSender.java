package com.genymobile.scrcpy;

import java.io.IOException;

public final class DeviceMessageSender {

    private final ControlChannel controlChannel;

    private Thread thread;

    private String clipboardText;

    private long ack;

    public DeviceMessageSender(ControlChannel controlChannel) {
        this.controlChannel = controlChannel;
    }

    public synchronized void pushClipboardText(String text) {
        clipboardText = text;
        notify();
    }

    public synchronized void pushAckClipboard(long sequence) {
        ack = sequence;
        notify();
    }

    private void loop() throws IOException, InterruptedException {
        while (!Thread.currentThread().isInterrupted()) {
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
                controlChannel.send(event);
            }
            if (text != null) {
                DeviceMessage event = DeviceMessage.createClipboard(text);
                controlChannel.send(event);
            }
        }
    }

    public void start() {
        thread = new Thread(() -> {
            try {
                loop();
            } catch (IOException | InterruptedException e) {
                // this is expected on close
            } finally {
                Ln.d("Device message sender stopped");
            }
        }, "control-send");
        thread.start();
    }

    public void stop() {
        if (thread != null) {
            thread.interrupt();
        }
    }

    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }
}
