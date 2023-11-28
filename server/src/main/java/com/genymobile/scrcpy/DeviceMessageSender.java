package com.genymobile.scrcpy;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

import java.io.IOException;

public final class DeviceMessageSender {
    private static final int MSG_CLIPBOARD_TEXT = 0;
    private static final int MSG_ACK_CLIPBOARD = 1;
    private static final int MSG_UHID_DATA = 2;

    private final DesktopConnection connection;

    private final HandlerThread handlerThread = new HandlerThread("DeviceMessageSender");
    private DeviceMessageSenderHandler handler;

    public DeviceMessageSender(DesktopConnection connection) {
        this.connection = connection;
    }

    public void start() {
        handlerThread.start();
        handler = new DeviceMessageSenderHandler(handlerThread.getLooper());
    }

    public void stop() {
        handlerThread.quitSafely();
    }

    public void join() {
        try {
            handlerThread.join();
        } catch (InterruptedException e) {
            Ln.e("Failed to join DeviceMessageSender thread: ", e);
        }
    }

    public void pushClipboardText(String text) {
        handler.sendMessage(handler.obtainMessage(MSG_CLIPBOARD_TEXT, text));
    }

    public void pushAckClipboard(long sequence) {
        handler.sendMessage(handler.obtainMessage(MSG_ACK_CLIPBOARD, sequence));
    }

    public void pushUHidData(int id, byte[] data) {
        handler.sendMessage(handler.obtainMessage(MSG_UHID_DATA, id, 0, data));
    }

    class DeviceMessageSenderHandler extends Handler {
        public DeviceMessageSenderHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            try {
                DeviceMessage event;
                switch (msg.what) {
                    case MSG_CLIPBOARD_TEXT:
                        event = DeviceMessage.createAckClipboard((long) msg.obj);
                        connection.sendDeviceMessage(event);
                        break;
                    case MSG_ACK_CLIPBOARD:
                        event = DeviceMessage.createClipboard((String) msg.obj);
                        connection.sendDeviceMessage(event);
                        break;
                    case MSG_UHID_DATA:
                        event = DeviceMessage.createUHidData(msg.arg1, (byte[]) msg.obj);
                        connection.sendDeviceMessage(event);
                        break;
                }
            } catch (IOException e) {
                Ln.e("Failed to send device message: ", e);
            }
        }
    }
}
