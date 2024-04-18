package com.genymobile.scrcpy;

import java.io.IOException;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

public final class DeviceMessageSender {

    private final Connection connection;
   // private final ControlChannel controlChannel;

    private String clipboardText;

    private Thread thread;
    private final BlockingQueue<DeviceMessage> queue = new ArrayBlockingQueue<>(16);

    public DeviceMessageSender(Connection connection) {
        this.connection = connection;
    }
    
    // public DeviceMessageSender(ControlChannel controlChannel) {
    //     this.controlChannel = controlChannel;
    // }

    public synchronized void pushClipboardText(String text) {
        clipboardText = text;
        notify();
    }

    public void send(DeviceMessage msg) {
        if (!queue.offer(msg)) {
            Ln.w("Device message dropped: " + msg.getType());
        }
    }

    // public void loop() throws IOException, InterruptedException {
    //     while (!Thread.currentThread().isInterrupted()) {
    //         DeviceMessage msg = queue.take();
    //         controlChannel.send(msg);
    //     }
    // }

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
