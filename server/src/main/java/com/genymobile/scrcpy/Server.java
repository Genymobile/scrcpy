package com.genymobile.scrcpy;

import android.content.pm.ApplicationInfo;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
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
        if (args.length != 6) {
            throw new IllegalArgumentException("Expecting 6 parameters");
        }

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

        boolean control = Boolean.parseBoolean(args[5]);
        options.setControl(control);

        return options;
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

    public static void main(String... args) throws Exception {
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                Ln.e("Exception on thread " + t, e);
            }
        });

        Ln.i("args = " + Arrays.asList(args));

        if (Build.PRODUCT.contains("meizu_16th")) {
            workaroundFor16th();
        }

        unlinkSelf();
        Options options = createOptions(args);
        scrcpy(options);

    }

    private static void workaroundFor16th() {
        Ln.d("workaroundFor16th in");
        Looper.prepareMainLooper();

        // mock to new a ActivityThread instance
        try {
            Class<?> ActivityThreadClass = Class.forName("android.app.ActivityThread");
            Method currentPackageName = ActivityThreadClass.getMethod("currentPackageName");
            currentPackageName.setAccessible(true);

            Field sCurrentActivityThreadField = ActivityThreadClass.getDeclaredField("sCurrentActivityThread");
            Field mBoundApplicationField = ActivityThreadClass.getDeclaredField("mBoundApplication");

            sCurrentActivityThreadField.setAccessible(true);
            mBoundApplicationField.setAccessible(true);

            Constructor<?> activityThreadConstructor = ActivityThreadClass.getDeclaredConstructor();
            activityThreadConstructor.setAccessible(true);
            sCurrentActivityThreadField.set(null, activityThreadConstructor.newInstance());
            Object sCurrentActivityThreadObject = sCurrentActivityThreadField.get(null);

            Class<?> AppBindDataClass = Class.forName("android.app.ActivityThread$AppBindData");
            Field appInfo = AppBindDataClass.getDeclaredField("appInfo");
            appInfo.setAccessible(true);
            Constructor<?> appBindDataConstructor = AppBindDataClass.getDeclaredConstructor();
            appBindDataConstructor.setAccessible(true);
            Object appBindDataObject = appBindDataConstructor.newInstance();
            ApplicationInfo applicationInfo = new ApplicationInfo();
            applicationInfo.packageName = "mock.pkg";
            appInfo.set(appBindDataObject, applicationInfo);
            mBoundApplicationField.setAccessible(true);
            mBoundApplicationField.set(sCurrentActivityThreadObject, appBindDataObject);
        } catch (Throwable throwable) {
            throwable.printStackTrace();
        }
    }
}
