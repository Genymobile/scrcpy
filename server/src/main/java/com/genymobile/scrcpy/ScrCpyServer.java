package com.genymobile.scrcpy;

import java.io.IOException;

public class ScrCpyServer {

    private static void scrcpy(Options options) throws IOException {
        final Device device = new Device(options);
        try (DesktopConnection connection = DesktopConnection.open(device)) {
            ScreenEncoder screenEncoder = new ScreenEncoder();

            // asynchronous
            startEventController(device, connection);

            try {
                // synchronous
                screenEncoder.streamScreen(device, connection.getOutputStream());
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
                    Ln.e("Exception from event controller", e);
                }
            }
        }).start();
    }

    private static Options createOptions(String... args) {
        Options options = new Options();
        if (args.length > 0) {
            int maxSize = Integer.parseInt(args[0]) & ~7; // multiple of 8
            options.setMaxSize(maxSize);
        }
        return options;
    }

    public static void main(String... args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                Ln.e("Exception on thread " + t, e);
            }
        });

        Options options = createOptions(args);
        scrcpy(options);
    }
}
