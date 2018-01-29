package com.genymobile.scrcpy;

import java.io.IOException;
import java.io.InterruptedIOException;

public class ScreenStreamer {

    private final Device device;
    private final DesktopConnection connection;
    private ScreenStreamerSession currentStreamer; // protected by 'this'

    public ScreenStreamer(Device device, DesktopConnection connection) {
        this.device = device;
        this.connection = connection;
    }

    private synchronized ScreenStreamerSession newScreenStreamerSession() {
        currentStreamer = new ScreenStreamerSession(device, connection);
        return currentStreamer;
    }

    public void streamScreen() throws IOException {
        while (true) {
            try {
                ScreenStreamerSession screenStreamer = newScreenStreamerSession();
                screenStreamer.streamScreen();
            } catch (InterruptedIOException e) {
                // the current screenrecord process has probably been killed due to reset(), start a new one without failing
            }
        }
    }

    public synchronized void reset() {
        if (currentStreamer != null) {
            // it will stop the blocking call to streamScreen(), so a new streamer will be started
            currentStreamer.stop();
        }
    }
}
