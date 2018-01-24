package com.genymobile.scrcpy;

import java.io.IOException;

public class ScrCpyServer {

    private static final String TAG = "scrcpy";

    private static void scrcpy() throws IOException {
        final Device device = new Device();
        try (DesktopConnection connection = DesktopConnection.open(device)) {
            final ScreenStreamer streamer = new ScreenStreamer(connection);
            device.setRotationListener(new Device.RotationListener() {
                @Override
                public void onRotationChanged(int rotation) {
                    streamer.reset();
                }
            });

            // asynchronous
            startEventController(device, connection);

            try {
                // synchronous
                streamer.streamScreen();
            } catch (IOException e) {
                Ln.e("Screen streaming interrupted", e);
            }
        }
    }

    private static void startEventController(final Device device, final DesktopConnection connection) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    new EventController(device, connection).control();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }).start();
    }

    public static void main(String... args) throws Exception {
        try {
            scrcpy();
        } catch (Throwable t) {
            t.printStackTrace();
            throw t;
        }
    }
}
