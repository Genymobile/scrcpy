package com.genymobile.scrcpy;

import android.annotation.SuppressLint;
import android.os.IBinder;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.lang.reflect.Method;

/**
 * On Android 14, the methods used to turn the device screen off have been moved from SurfaceControl (in framework.jar) to DisplayControl (a system
 * server class). As a consequence, they could not be called directly. See {@url https://github.com/Genymobile/scrcpy/issues/3927}.
 * <p>
 * Instead, run a separate process with a different classpath and LD_PRELOAD just to set the display power mode. The scrcpy server can request to
 * this process to set the display mode by writing the mode (a single byte, the value of one of the SurfaceControl.POWER_MODE_* constants,
 * typically 0=off, 2=on) to the process stdin. In return, it receives the status of the request (0=ok, 1=error) on the process stdout.
 * <p>
 * This separate process is started on the first display mode request.
 * <p>
 * Since the client must not block, and calling/joining a process is blocking (this specific one takes a few hundred milliseconds to complete),
 * this class uses an internal thread to execute the requests asynchronously, and serialize them (so that two successive requests are guaranteed to
 * be executed in order). In addition, it only executes the last pending request (setting power mode to value X then to value Y is equivalent to
 * just setting it to value Y).
 */
public final class DisplayPowerMode {

    private static final Proxy PROXY = new Proxy();

    private static final class Proxy implements Runnable {

        private Process process;
        private Thread thread;
        private int requestedMode = -1;
        private boolean stopped;

        synchronized boolean requestMode(int mode) {
            try {
                if (process == null) {
                    process = executeDisplayPowerModeDaemon();
                    thread = new Thread(this, "DisplayPowerModeProxy");
                    thread.setDaemon(true);
                    thread.start();
                }
                requestedMode = mode;
                notify();
                return true;
            } catch (IOException e) {
                Ln.e("Could not start display power mode daemon", e);
                return false;
            }
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
                OutputStream out = process.getOutputStream();
                InputStream in = process.getInputStream();
                while (true) {
                    int mode;
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
                        out.write(mode);
                        out.flush();
                        int status = in.read();
                        if (status != 0) {
                            Ln.e("Set display power mode failed remotely: status=" + status);
                        }
                    } catch (IOException e) {
                        Ln.e("Could not request display power mode", e);
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
    public static boolean setRemoteDisplayPowerMode(int mode) {
        return PROXY.requestMode(mode);
    }

    public static void stopAndJoin() {
        PROXY.stopAndJoin();
    }

    // Called from the proxy thread in the scrcpy process
    private static Process executeDisplayPowerModeDaemon() throws IOException {
        String[] ldPreloadLibs = {"/system/lib64/libandroid_servers.so"};
        String[] cmd = {"app_process", "/", DisplayPowerMode.class.getName()};

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
        // This process uses stdin/stdout to communicate with the caller, make sure nothing else writes to stdout
        // (and never use Ln methods other than Ln.w() and Ln.e()).
        PrintStream nullStream = new PrintStream(new Ln.NullOutputStream());
        System.setOut(nullStream);
        PrintStream stdout = Ln.CONSOLE_OUT;

        try {
            while (true) {
                // Wait for requests
                int request = System.in.read();
                if (request == -1) {
                    // EOF
                    return;
                }
                try {
                    setDisplayPowerModeUsingDisplayControl(request);
                    stdout.write(0); // ok
                } catch (Throwable e) {
                    Ln.e("Could not set display power mode", e);
                    stdout.write(1); // error
                }
                stdout.flush();
            }
        } catch (IOException e) {
            // Expected when the server is dead
        }
    }
}
