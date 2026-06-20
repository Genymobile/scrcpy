package com.genymobile.scrcpy;

import com.genymobile.scrcpy.device.Device;
import com.genymobile.scrcpy.util.Ln;
import com.genymobile.scrcpy.util.Settings;
import com.genymobile.scrcpy.util.SettingsException;
import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.os.BatteryManager;
import android.os.Looper;
import android.system.ErrnoException;
import android.system.Os;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;

/**
 * Handle the cleanup of scrcpy, even if the main process is killed.
 * <p>
 * This is useful to restore some state when scrcpy is closed, even on device disconnection (which kills the scrcpy process).
 */
public final class CleanUp {

    // Dynamic options
    private static final int PENDING_CHANGE_DISPLAY_POWER = 1 << 0;
    private int pendingChanges;
    private boolean pendingRestoreDisplayPower;

    private Thread thread;
    private boolean interrupted;

    private CleanUp(Options options) {
        thread = new Thread(() -> runCleanUp(options), "cleanup");
        thread.start();
    }

    public static CleanUp start(Options options) {
        return new CleanUp(options);
    }

    public synchronized void interrupt() {
        // Do not use thread.interrupt() because only the wait() call must be interrupted, not Command.exec()
        interrupted = true;
        notify();
    }

    public void join() throws InterruptedException {
        thread.join();
    }

    private void runCleanUp(Options options) {
        boolean disableShowTouches = false;
        if (options.getShowTouches()) {
            try {
                String oldValue = Settings.getAndPutValue(Settings.TABLE_SYSTEM, "show_touches", "1");
                // If "show touches" was disabled, it must be disabled back on clean up
                disableShowTouches = !"1".equals(oldValue);
            } catch (SettingsException e) {
                Ln.e("Could not change \"show_touches\"", e);
            }
        }

        int restoreStayOn = -1;
        if (options.getStayAwake()) {
            int stayOn = BatteryManager.BATTERY_PLUGGED_AC | BatteryManager.BATTERY_PLUGGED_USB | BatteryManager.BATTERY_PLUGGED_WIRELESS;
            try {
                String oldValue = Settings.getAndPutValue(Settings.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(stayOn));
                try {
                    int currentStayOn = Integer.parseInt(oldValue);
                    // Restore only if the current value is different
                    if (currentStayOn != stayOn) {
                        restoreStayOn = currentStayOn;
                    }
                } catch (NumberFormatException e) {
                    // ignore
                }
            } catch (SettingsException e) {
                Ln.e("Could not change \"stay_on_while_plugged_in\"", e);
            }
        }

        int restoreScreenOffTimeout = -1;
        int screenOffTimeout = options.getScreenOffTimeout();
        if (screenOffTimeout != -1) {
            try {
                String oldValue = Settings.getAndPutValue(Settings.TABLE_SYSTEM, "screen_off_timeout", String.valueOf(screenOffTimeout));
                try {
                    int currentScreenOffTimeout = Integer.parseInt(oldValue);
                    // Restore only if the current value is different
                    if (currentScreenOffTimeout != screenOffTimeout) {
                        restoreScreenOffTimeout = currentScreenOffTimeout;
                    }
                } catch (NumberFormatException e) {
                    // ignore
                }
            } catch (SettingsException e) {
                Ln.e("Could not change \"screen_off_timeout\"", e);
            }
        }

        int displayId = options.getDisplayId();

        int restoreDisplayImePolicy = -1;
        if (displayId > 0) {
            int displayImePolicy = options.getDisplayImePolicy();
            if (displayImePolicy != -1) {
                int currentDisplayImePolicy = ServiceManager.getWindowManager().getDisplayImePolicy(displayId);
                if (currentDisplayImePolicy != displayImePolicy) {
                    ServiceManager.getWindowManager().setDisplayImePolicy(displayId, displayImePolicy);
                    restoreDisplayImePolicy = currentDisplayImePolicy;
                }
            }
        }

        boolean powerOffScreen = options.getPowerOffScreenOnClose();

        try {
            run(displayId, restoreStayOn, disableShowTouches, powerOffScreen, restoreScreenOffTimeout, restoreDisplayImePolicy);
        } catch (IOException e) {
            Ln.e("Clean up I/O exception", e);
        }
    }

    private void run(int displayId, int restoreStayOn, boolean disableShowTouches, boolean powerOffScreen, int restoreScreenOffTimeout,
            int restoreDisplayImePolicy) throws IOException {
        String[] cmd = {
                "app_process",
                "/",
                CleanUp.class.getName(),
                String.valueOf(displayId),
                String.valueOf(restoreStayOn),
                String.valueOf(disableShowTouches),
                String.valueOf(powerOffScreen),
                String.valueOf(restoreScreenOffTimeout),
                String.valueOf(restoreDisplayImePolicy),
        };

        ProcessBuilder builder = new ProcessBuilder(cmd);
        builder.environment().put("CLASSPATH", Server.SERVER_PATH);
        Process process = builder.start();
        OutputStream out = process.getOutputStream();

        while (true) {
            int localPendingChanges;
            boolean localPendingRestoreDisplayPower;
            synchronized (this) {
                while (!interrupted && pendingChanges == 0) {
                    try {
                        wait();
                    } catch (InterruptedException e) {
                        throw new AssertionError("Clean up thread MUST NOT be interrupted");
                    }
                }
                if (interrupted) {
                    break;
                }
                localPendingChanges = pendingChanges;
                localPendingRestoreDisplayPower = pendingRestoreDisplayPower;
                pendingChanges = 0;
            }
            if ((localPendingChanges & PENDING_CHANGE_DISPLAY_POWER) != 0) {
                out.write(localPendingRestoreDisplayPower ? 1 : 0);
                out.flush();
            }
        }
    }

    public synchronized void setRestoreDisplayPower(boolean restoreDisplayPower) {
        pendingRestoreDisplayPower = restoreDisplayPower;
        pendingChanges |= PENDING_CHANGE_DISPLAY_POWER;
        notify();
    }

    public static void unlinkSelf() {
        try {
            new File(Server.SERVER_PATH).delete();
        } catch (Exception e) {
            Ln.e("Could not unlink server", e);
        }
    }

    @SuppressWarnings("deprecation")
    private static void prepareMainLooper() {
        Looper.prepareMainLooper();
    }

    public static void main(String... args) {
        try {
            // Start a new session to avoid being terminated along with the server process on some devices
            Os.setsid();
        } catch (ErrnoException e) {
            Ln.e("setsid() failed", e);
        }
        unlinkSelf();

        // Needed for workarounds
        prepareMainLooper();
        Workarounds.apply();

        int displayId = Integer.parseInt(args[0]);
        int restoreStayOn = Integer.parseInt(args[1]);
        boolean disableShowTouches = Boolean.parseBoolean(args[2]);
        boolean powerOffScreen = Boolean.parseBoolean(args[3]);
        int restoreScreenOffTimeout = Integer.parseInt(args[4]);
        int restoreDisplayImePolicy = Integer.parseInt(args[5]);

        // Dynamic option
        boolean restoreDisplayPower = false;

        try {
            // Wait for the server to die
            int msg;
            while ((msg = System.in.read()) != -1) {
                // Only restore display power
                assert msg == 0 || msg == 1;
                restoreDisplayPower = msg != 0;
            }
        } catch (IOException e) {
            // Expected when the server is dead
        }

        Ln.i("Cleaning up");

        if (disableShowTouches) {
            Ln.i("Disabling \"show touches\"");
            try {
                Settings.putValue(Settings.TABLE_SYSTEM, "show_touches", "0");
            } catch (SettingsException e) {
                Ln.e("Could not restore \"show_touches\"", e);
            }
        }

        if (restoreStayOn != -1) {
            Ln.i("Restoring \"stay awake\"");
            try {
                Settings.putValue(Settings.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(restoreStayOn));
            } catch (SettingsException e) {
                Ln.e("Could not restore \"stay_on_while_plugged_in\"", e);
            }
        }

        if (restoreScreenOffTimeout != -1) {
            Ln.i("Restoring \"screen off timeout\"");
            try {
                Settings.putValue(Settings.TABLE_SYSTEM, "screen_off_timeout", String.valueOf(restoreScreenOffTimeout));
            } catch (SettingsException e) {
                Ln.e("Could not restore \"screen_off_timeout\"", e);
            }
        }

        if (restoreDisplayImePolicy != -1) {
            Ln.i("Restoring \"display IME policy\"");
            ServiceManager.getWindowManager().setDisplayImePolicy(displayId, restoreDisplayImePolicy);
        }

        // Change the power of the main display when mirroring a virtual display
        int targetDisplayId = displayId != Device.DISPLAY_ID_NONE ? displayId : 0;
        if (Device.isScreenOn(targetDisplayId)) {
            if (powerOffScreen) {
                Ln.i("Power off screen");
                Device.powerOffScreen(targetDisplayId);
            } else if (restoreDisplayPower) {
                Ln.i("Restoring display power");
                Device.setDisplayPower(targetDisplayId, true);
            }
        }

        System.exit(0);
    }
}
