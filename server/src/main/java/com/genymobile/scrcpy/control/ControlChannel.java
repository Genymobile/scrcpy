package com.genymobile.scrcpy.control;

import android.net.LocalSocket;

import java.io.IOException;

public final class ControlChannel {

    private final LocalSocket controlSocket;
    private final ControlMessageReader reader;
    private final DeviceMessageWriter writer;

    public ControlChannel(LocalSocket controlSocket) throws IOException {
        this.controlSocket = controlSocket;
        reader = new ControlMessageReader(controlSocket.getInputStream());
        writer = new DeviceMessageWriter(controlSocket.getOutputStream());
    }

    public void shutdownInput() throws IOException {
        controlSocket.shutdownInput();
    }

    public ControlMessage recv() throws IOException {
        return reader.read();
    }

    public void send(DeviceMessage msg) throws IOException {
        writer.write(msg);
    }
}
