package com.genymobile.scrcpy;

import android.graphics.Rect;
import android.media.MediaCodec;
import android.os.Build;

import java.io.File;
import java.io.IOException;

public final class Server {

    private static final String SERVER_PATH = "/data/local/tmp/scrcpy-server.jar";

    private Server() {
        // not instantiable
    }

    private static void scrcpy(Options options) throws IOException {
        final Device device = new Device(options);
        boolean tunnelForward = options.isTunnelForward();
        try (DesktopConnection connection = DesktopConnection.open(device, tunnelForward)) {
            ScreenEncoder screenEncoder = new ScreenEncoder(options.getQuality(), options.getMaxFps(), options.getScale());

            if (options.getControl()) {
                Controller controller = new Controller(device, connection);

                // asynchronous
                startController(controller);
                startDeviceMessageSender(controller.getSender());
            }

            try {
                // synchronous
                screenEncoder.streamScreen(device, connection.getVideoFd());
            } catch (IOException e) {
                // this is expected on close
                Ln.d("Screen streaming stopped");
            }
        }
    }

    private static void startController(final Controller controller) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    controller.control();
                } catch (IOException e) {
                    // this is expected on close
                    Ln.d("Controller stopped");
                    Ln.d("E:" + e.getMessage());
                }
            }
        }).start();
    }

    private static void startDeviceMessageSender(final DeviceMessageSender sender) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    sender.loop();
                } catch (IOException | InterruptedException e) {
                    // this is expected on close
                    Ln.d("Device message sender stopped");
                }
            }
        }).start();
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static Options createOptions(String... args) {
        if (args.length < 1) {
            throw new IllegalArgumentException("Missing client version");
        }

        String clientVersion = args[0];
        Ln.i("VERSION_NAME: " + BuildConfig.VERSION_NAME);
        if (!clientVersion.equals(BuildConfig.VERSION_NAME)) {
            throw new IllegalArgumentException(
                    "The server version (" + clientVersion + ") does not match the client " + "(" + BuildConfig.VERSION_NAME + ")");
        }

        if (args.length != 8) {
            throw new IllegalArgumentException("Expecting 8 parameters");
        }

        Options options = new Options();

        int maxSize = Integer.parseInt(args[1]) & ~7; // multiple of 8
        options.setMaxSize(maxSize);

        int bitRate = Integer.parseInt(args[2]);
        options.setBitRate(bitRate);

        int maxFps = Integer.parseInt(args[3]);
        options.setMaxFps(maxFps);

        // use "adb forward" instead of "adb tunnel"? (so the server must listen)
        boolean tunnelForward = Boolean.parseBoolean(args[4]);
        options.setTunnelForward(tunnelForward);

        Rect crop = parseCrop(args[5]);
        options.setCrop(crop);

        boolean sendFrameMeta = Boolean.parseBoolean(args[6]);
        options.setSendFrameMeta(sendFrameMeta);

        boolean control = Boolean.parseBoolean(args[7]);
        options.setControl(control);

        return options;
    }

    private static Options customOptions(String... args) {
        org.apache.commons.cli.CommandLine commandLine = null;
        org.apache.commons.cli.CommandLineParser parser = new org.apache.commons.cli.BasicParser();
        org.apache.commons.cli.Options options = new org.apache.commons.cli.Options();
        options.addOption("Q", true, "JPEG quality (0-100)");
        options.addOption("r", true, "Frame rate (frames/s)");
        options.addOption("P", true, "Display projection (scale 1,2,4...)");
        options.addOption("h", false, "Show help");
        try {
            commandLine = parser.parse(options, args);
        } catch (Exception e) {
            Ln.e(e.getMessage());
            System.exit(0);
        }

        if (commandLine.hasOption('h')) {
            System.out.println(
                    "Usage: %s [-h]\n"
                            + "  -Q <value>:    JPEG quality (0-100).\n"
                            + "  -r <value>:    Frame rate (frames/s).\n"
                            + "  -P <value>:    Display projection (scale 1,2,4...).\n"
                            + "  -h:            Show help.\n"
            );
            System.exit(0);
        }

        Options o = new Options();
        o.setMaxSize(0);
        o.setBitRate(1000000);
        o.setTunnelForward(true);
        o.setCrop(null);
        o.setSendFrameMeta(true);
        o.setControl(true);
        o.setMaxFps(24);
        o.setQuality(50);
        o.setScale(2);
        if (commandLine.hasOption('Q')) {
            int i = 0;
            try {
                i = Integer.parseInt(commandLine.getOptionValue('Q'));
            } catch (Exception e) {
            }
            if (i > 0 && i <= 100) {
                o.setQuality(i);
            }
        }
        if (commandLine.hasOption('r')) {
            int i = 0;
            try {
                i = Integer.parseInt(commandLine.getOptionValue('r'));
            } catch (Exception e) {
            }
            if (i > 0 && i <= 100) {
                o.setMaxFps(i);
            }
        }
        if (commandLine.hasOption('P')) {
            int i = 0;
            try {
                i = Integer.parseInt(commandLine.getOptionValue('P'));
            } catch (Exception e) {
            }
            if (i > 0) {
                o.setScale(i);
            }
        }
        return o;
    }

    @SuppressWarnings("checkstyle:MagicNumber")
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
            Ln.e("Could not unlink server", e);
        }
    }

    @SuppressWarnings("checkstyle:MagicNumber")
    private static void suggestFix(Throwable e) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (e instanceof MediaCodec.CodecException) {
                MediaCodec.CodecException mce = (MediaCodec.CodecException) e;
                if (mce.getErrorCode() == 0xfffffc0e) {
                    Ln.e("The hardware encoder is not able to encode at the given definition.");
                    Ln.e("Try with a lower definition:");
                    Ln.e("    scrcpy -m 1024");
                }
            }
        }
    }

    public static void main(String... args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                Ln.e("Exception on thread " + t, e);
                suggestFix(e);
            }
        });

//        unlinkSelf();
//        Options options = createOptions(args);
        Options options = customOptions(args);
        Ln.i("Options frame rate: " + options.getMaxFps() + " (1 ~ 100)");
        Ln.i("Options quality: " + options.getQuality() + " (1 ~ 100)");
        Ln.i("Options scale: " + options.getScale() + " (1,2,4...)");
        scrcpy(options);
    }
}
