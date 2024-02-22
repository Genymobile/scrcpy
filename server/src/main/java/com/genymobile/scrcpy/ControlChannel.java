package com.genymobile.scrcpy;

import android.net.LocalSocket;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public final class ControlChannel {
    private final InputStream inputStream;
    private final OutputStream outputStream;

    private final ControlMessageReader reader = new ControlMessageReader();
    private final DeviceMessageWriter writer = new DeviceMessageWriter();

    public ControlChannel(LocalSocket controlSocket) throws IOException {
        this.inputStream = controlSocket.getInputStream();
        this.outputStream = controlSocket.getOutputStream();
    }

    public ControlMessage recv() throws IOException {
        ControlMessage msg = reader.next();
        while (msg == null) {
            reader.readFrom(inputStream);
            msg = reader.next();
        }
        return msg;
    }

    public void send(DeviceMessage msg) throws IOException {
        writer.writeTo(msg, outputStream);
    }
}
