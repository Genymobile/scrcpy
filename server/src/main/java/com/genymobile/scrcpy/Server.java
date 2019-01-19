package com.genymobile.scrcpy;

import android.graphics.Rect;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;

public final class Server {

    private static final String SERVER_PATH = "/data/local/tmp/scrcpy-server.jar";

    private Server() {
        // not instantiable
    }

    private static void scrcpy(Options options) throws IOException {
        final Device device = new Device(options);
        boolean tunnelForward = options.isTunnelForward();
        try (DesktopConnection connection = DesktopConnection.open(device, tunnelForward)) {
            ScreenEncoder screenEncoder = new ScreenEncoder(options.getSendFrameMeta(), options.getBitRate());

            // asynchronous
            startEventController(device, connection);

            try {
                // synchronous
                screenEncoder.streamScreen(device, connection.getFd());
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
        if (args.length != 5)
            throw new IllegalArgumentException("Expecting 5 parameters");

        Options options = new Options();

        int maxSize = Integer.parseInt(args[0]) & ~7; // multiple of 8
        options.setMaxSize(maxSize);

        int bitRate = Integer.parseInt(args[1]);
        options.setBitRate(bitRate);

        // use "adb forward" instead of "adb tunnel"? (so the server must listen)
        boolean tunnelForward = Boolean.parseBoolean(args[2]);
        options.setTunnelForward(tunnelForward);

        Rect crop = parseCrop(args[3]);
        options.setCrop(crop);

        boolean sendFrameMeta = Boolean.parseBoolean(args[4]);
        options.setSendFrameMeta(sendFrameMeta);

        return options;
    }

    private static Rect parseCrop(String crop) {
        if ("-".equals(crop)) {
            return null;
        }
        // input format: "width:height:x:y"
        String[] tokens = crop.split(":");
        if (tokens.length != 4) {
            throw new IllegalArgumentException("Crop must contains 4 values separated by colons: \"" + crop + "\"");
        }
        int width = Integer.parseInt(tokens[0]);
        int height = Integer.parseInt(tokens[1]);
        int x = Integer.parseInt(tokens[2]);
        int y = Integer.parseInt(tokens[3]);
        return new Rect(x, y, x + width, y + height);
    }

    private static void unlinkSelf() {
        try {
            new File(SERVER_PATH).delete();
        } catch (Exception e) {
            Ln.e("Cannot unlink server", e);
        }
    }

    public static void main(String... args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                Ln.e("Exception on thread " + t, e);
            }
        });

        unlinkSelf();
        Options options = createOptions(args);
        scrcpy(options);
    }
}
