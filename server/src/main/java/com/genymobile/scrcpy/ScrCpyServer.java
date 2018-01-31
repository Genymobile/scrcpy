package com.genymobile.scrcpy;

import java.io.IOException;

public class ScrCpyServer {

    private static final String TAG = "scrcpy";

    private static void scrcpy(Options options) throws IOException {
        final Device device = new Device(options);
        try (DesktopConnection connection = DesktopConnection.open(device)) {
            final ScreenStreamer streamer = new ScreenStreamer(device, connection);
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

    private static Options createOptions(String... args) {
        Options options = new Options();
        if (args.length > 0) {
            int maximumSize = Integer.parseInt(args[0]) & ~7; // multiple of 8
            options.setMaximumSize(maximumSize);
        }
        return options;
    }

    public static void main(String... args) throws Exception {
        Options options = createOptions(args);
        try {
            scrcpy(options);
        } catch (Throwable t) {
            t.printStackTrace();
            throw t;
        }
    }
}
