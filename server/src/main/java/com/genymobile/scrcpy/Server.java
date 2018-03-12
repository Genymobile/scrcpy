package com.genymobile.scrcpy;

import java.io.IOException;

public final class Server {

    private Server() {
        // not instantiable
    }

    private static void scrcpy(Options options) throws IOException {
        final Device device = new Device(options);
        boolean tunnelForward = options.isTunnelForward();
        try (DesktopConnection connection = DesktopConnection.open(device, tunnelForward)) {
            ScreenEncoder screenEncoder = new ScreenEncoder(options.getBitRate());

            // asynchronous
            startEventController(device, connection);

            try {
                // synchronous
                screenEncoder.streamScreen(device, connection.getOutputStream());
            } catch (IOException e) {
                // this is expected on close
                Ln.d("Screen streaming stopped");
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
                    // this is expected on close
                    Ln.d("Event controller stopped");
                }
            }
        }).start();
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static Options createOptions(String... args) {
        Options options = new Options();
        if (args.length < 1) {
            return options;
        }
        int maxSize = Integer.parseInt(args[0]) & ~7; // multiple of 8
        options.setMaxSize(maxSize);

        if (args.length < 2) {
            return options;
        }
        int bitRate = Integer.parseInt(args[1]);
        options.setBitRate(bitRate);

        if (args.length < 3) {
            return options;
        }
        // use "adb forward" instead of "adb tunnel"? (so the server must listen)
        boolean tunnelForward = Boolean.parseBoolean(args[2]);
        options.setTunnelForward(tunnelForward);

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
