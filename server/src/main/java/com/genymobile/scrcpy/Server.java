package com.genymobile.scrcpy;

import android.graphics.Rect;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.os.Build;

import java.util.Locale;

public final class Server {


    private Server() {
        // not instantiable
    }

    private static void parseArguments(Options options, VideoSettings videoSettings, String... args) {
        if (args.length < 1) {
            throw new IllegalArgumentException("Missing client version");
        }

        String clientVersion = args[0];
        if (!clientVersion.equals(BuildConfig.VERSION_NAME)) {
            throw new IllegalArgumentException(
                    "The server version (" + BuildConfig.VERSION_NAME + ") does not match the client " + "(" + clientVersion + ")");
        }

        if (args[1].toLowerCase().equals("web")) {
            options.setServerType(Options.TYPE_WEB_SOCKET);
            if (args.length > 2) {
                Ln.Level level = Ln.Level.valueOf(args[2].toUpperCase(Locale.ENGLISH));
                options.setLogLevel(level);
            }
            if (args.length > 3) {
                int portNumber = Integer.parseInt(args[3]);
                options.setPortNumber(portNumber);
            }
            if (args.length > 4) {
                boolean listenOnAllInterfaces = Boolean.parseBoolean(args[4]);
                options.setListenOnAllInterfaces(listenOnAllInterfaces);
            }
            return;
        }

        final int expectedParameters = 16;
        if (args.length != expectedParameters) {
            throw new IllegalArgumentException("Expecting " + expectedParameters + " parameters");
        }

        Ln.Level level = Ln.Level.valueOf(args[1].toUpperCase(Locale.ENGLISH));
        options.setLogLevel(level);

        int maxSize = Integer.parseInt(args[2]);
        if (maxSize != 0) {
            videoSettings.setBounds(maxSize, maxSize);
        }

        int bitRate = Integer.parseInt(args[3]);
        videoSettings.setBitRate(bitRate);

        int maxFps = Integer.parseInt(args[4]);
        videoSettings.setMaxFps(maxFps);

        int lockedVideoOrientation = Integer.parseInt(args[5]);
        videoSettings.setLockedVideoOrientation(lockedVideoOrientation);

        // use "adb forward" instead of "adb tunnel"? (so the server must listen)
        boolean tunnelForward = Boolean.parseBoolean(args[6]);
        options.setTunnelForward(tunnelForward);

        Rect crop = parseCrop(args[7]);
        videoSettings.setCrop(crop);

        boolean sendFrameMeta = Boolean.parseBoolean(args[8]);
        videoSettings.setSendFrameMeta(sendFrameMeta);

        boolean control = Boolean.parseBoolean(args[9]);
        options.setControl(control);

        int displayId = Integer.parseInt(args[10]);
        videoSettings.setDisplayId(displayId);

        boolean showTouches = Boolean.parseBoolean(args[11]);
        options.setShowTouches(showTouches);

        boolean stayAwake = Boolean.parseBoolean(args[12]);
        options.setStayAwake(stayAwake);

        String codecOptions = args[13];
        options.setCodecOptions(codecOptions);
        videoSettings.setCodecOptions(codecOptions);

        String encoderName = "-".equals(args[14]) ? null : args[14];
        videoSettings.setEncoderName(encoderName);

        boolean powerOffScreenOnClose = Boolean.parseBoolean(args[15]);
        options.setPowerOffScreenOnClose(powerOffScreenOnClose);
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
        if (e instanceof InvalidDisplayIdException) {
            InvalidDisplayIdException idie = (InvalidDisplayIdException) e;
            int[] displayIds = idie.getAvailableDisplayIds();
            if (displayIds != null && displayIds.length > 0) {
                Ln.e("Try to use one of the available display ids:");
                for (int id : displayIds) {
                    Ln.e("    scrcpy --display " + id);
                }
            }
        } else if (e instanceof InvalidEncoderException) {
            InvalidEncoderException iee = (InvalidEncoderException) e;
            MediaCodecInfo[] encoders = iee.getAvailableEncoders();
            if (encoders != null && encoders.length > 0) {
                Ln.e("Try to use one of the available encoders:");
                for (MediaCodecInfo encoder : encoders) {
                    Ln.e("    scrcpy --encoder '" + encoder.getName() + "'");
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

        Options options = new Options();
        VideoSettings videoSettings = new VideoSettings();
        parseArguments(options, videoSettings, args);
        Ln.initLogLevel(options.getLogLevel());
        if (options.getServerType() == Options.TYPE_LOCAL_SOCKET) {
            new DesktopConnection(options, videoSettings);
        } else if (options.getServerType() == Options.TYPE_WEB_SOCKET) {
            WSServer wsServer = new WSServer(options);
            wsServer.setReuseAddr(true);
            wsServer.run();
        }
    }
}
