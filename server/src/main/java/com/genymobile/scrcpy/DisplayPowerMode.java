package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.os.IBinder;

import java.io.IOException;
import java.lang.reflect.Method;

/**
 * On Android 14, the methods used to turn the device screen off have been moved from SurfaceControl (in framework.jar) to DisplayControl (a system
 * server class). As a consequence, they could not be called directly. See {@url https://github.com/Genymobile/scrcpy/issues/3927}.
 * <p>
 * Instead, run a separate process with a different classpath and LD_PRELOAD just to set the display power mode.
 * <p>
 * Since the client must not block, and calling/joining a process is blocking (this specific one takes a few hundred milliseconds to complete),
 * this class uses an internal thread to execute the requests asynchronously, and serialize them (so that two successive requests are guaranteed to
 * be executed in order). In addition, it only executes the last pending request (setting power mode to value X then to value Y is equivalent to
 * just setting it to value Y).
 */
public final class DisplayPowerMode {

    private static final Proxy PROXY = new Proxy();

    private static final class Proxy implements Runnable {

        private Thread thread;
        private int requestedMode = -1;
        private boolean stopped;

        synchronized void requestMode(int mode) {
            if (thread == null) {
                thread = new Thread(this, "DisplayPowerModeProxy");
                thread.setDaemon(true);
                thread.start();
            }
            requestedMode = mode;
            notify();
        }

        void stopAndJoin() {
            boolean hasThread;
            synchronized (this) {
                hasThread = thread != null;
                if (thread != null) {
                    stopped = true;
                    notify();
                }
            }

            if (hasThread) {
                // Join the thread without holding the mutex (that would cause a deadlock)
                try {
                    thread.join();
                } catch (InterruptedException e) {
                    Ln.e("Thread join interrupted", e);
                }
            }
        }

        @Override
        public void run() {
            try {
                int mode;
                while (true) {
                    synchronized (this) {
                        while (!stopped && requestedMode == -1) {
                            wait();
                        }
                        mode = requestedMode;
                        requestedMode = -1;
                    }

                    // Even if stopped, the last request must be executed to restore the display power mode to normal
                    if (mode == -1) {
                        return;
                    }

                    try {
                        Process process = executeSystemServerSetDisplayPowerMode(mode);
                        int status = process.waitFor();
                        if (status != 0) {
                            Ln.e("Set display power mode failed remotely: status=" + status);
                        }
                    } catch (Exception e) {
                        Ln.e("Failed to execute process", e);
                    }
                }
            } catch (InterruptedException e) {
                // end of thread
            }
        }
    }

    private DisplayPowerMode() {
        // not instantiable
    }

    // Called from the scrcpy process
    public static void setRemoteDisplayPowerMode(int mode) {
        PROXY.requestMode(mode);
    }

    public static void stopAndJoin() {
        PROXY.stopAndJoin();
    }

    // Called from the proxy thread in the scrcpy process
    private static Process executeSystemServerSetDisplayPowerMode(int mode) throws IOException {
        String[] ldPreloadLibs = {"/system/lib64/libandroid_servers.so"};
        String[] cmd = {"app_process", "/", DisplayPowerMode.class.getName(), String.valueOf(mode)};

        ProcessBuilder builder = new ProcessBuilder(cmd);
        builder.environment().put("LD_PRELOAD", String.join(" ", ldPreloadLibs));
        builder.environment().put("CLASSPATH", Server.SERVER_PATH + ":/system/framework/services.jar");
        return builder.start();
    }

    // Executed in the DisplayPowerMode-specific process
    @SuppressLint({"PrivateApi", "SoonBlockedPrivateApi"})
    private static void setDisplayPowerModeUsingDisplayControl(int mode) throws Exception {
        System.loadLibrary("android_servers");

        @SuppressLint("PrivateApi")
        Class<?> displayControlClass = Class.forName("com.android.server.display.DisplayControl");
        Method getPhysicalDisplayIdsMethod = displayControlClass.getDeclaredMethod("getPhysicalDisplayIds");
        Method getPhysicalDisplayTokenMethod = displayControlClass.getDeclaredMethod("getPhysicalDisplayToken", long.class);

        Class<?> surfaceControlClass = Class.forName("android.view.SurfaceControl");
        Method setDisplayPowerModeMethod = surfaceControlClass.getDeclaredMethod("setDisplayPowerMode", IBinder.class, int.class);

        long[] displayIds = (long[]) getPhysicalDisplayIdsMethod.invoke(null);
        for (long displayId : displayIds) {
            Object token = getPhysicalDisplayTokenMethod.invoke(null, displayId);
            setDisplayPowerModeMethod.invoke(null, token, mode);
        }
    }

    public static void main(String... args) {
        Ln.disableSystemStreams();
        Ln.initLogLevel(Ln.Level.DEBUG);

        int status = run(args) ? 0 : 1;
        System.exit(status);
    }

    private static boolean run(String... args) {
        if (args.length != 1) {
            Ln.e("Exactly one argument expected: the value of one of the SurfaceControl.POWER_MODE_* constants (typically 0 or 2)");
            return false;
        }

        try {
            int mode = Integer.parseInt(args[0]);
            setDisplayPowerModeUsingDisplayControl(mode);
            return true;
        } catch (Throwable e) {
            Ln.e("Could not set display power mode", e);
            return false;
        }
    }
}
