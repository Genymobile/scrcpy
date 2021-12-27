package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ServiceManager;

import android.os.Parcel;
import android.os.Parcelable;
import android.util.Base64;

import java.io.File;
import java.io.IOException;

/**
 * Handle the cleanup of scrcpy, even if the main process is killed.
 * <p>
 * This is useful to restore some state when scrcpy is closed, even on device disconnection (which kills the scrcpy process).
 */
public final class CleanUp {

    public static final String SERVER_PATH = "/data/local/tmp/scrcpy-server.jar";

    // A simple struct to be passed from the main process to the cleanup process
    public static class Config implements Parcelable {

        public static final Creator<Config> CREATOR = new Creator<Config>() {
            @Override
            public Config createFromParcel(Parcel in) {
                return new Config(in);
            }

            @Override
            public Config[] newArray(int size) {
                return new Config[size];
            }
        };

        private static final int FLAG_DISABLE_SHOW_TOUCHES = 1;
        private static final int FLAG_RESTORE_NORMAL_POWER_MODE = 2;
        private static final int FLAG_POWER_OFF_SCREEN = 4;

        private int displayId;

        // Restore the value (between 0 and 7), -1 to not restore
        // <https://developer.android.com/reference/android/provider/Settings.Global#STAY_ON_WHILE_PLUGGED_IN>
        private int restoreStayOn = -1;

        private boolean disableShowTouches;
        private boolean restoreNormalPowerMode;
        private boolean powerOffScreen;

        public Config() {
            // Default constructor, the fields are initialized by CleanUp.configure()
        }

        protected Config(Parcel in) {
            displayId = in.readInt();
            restoreStayOn = in.readInt();
            byte options = in.readByte();
            disableShowTouches = (options & FLAG_DISABLE_SHOW_TOUCHES) != 0;
            restoreNormalPowerMode = (options & FLAG_RESTORE_NORMAL_POWER_MODE) != 0;
            powerOffScreen = (options & FLAG_POWER_OFF_SCREEN) != 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(displayId);
            dest.writeInt(restoreStayOn);
            byte options = 0;
            if (disableShowTouches) {
                options |= FLAG_DISABLE_SHOW_TOUCHES;
            }
            if (restoreNormalPowerMode) {
                options |= FLAG_RESTORE_NORMAL_POWER_MODE;
            }
            if (powerOffScreen) {
                options |= FLAG_POWER_OFF_SCREEN;
            }
            dest.writeByte(options);
        }

        private boolean hasWork() {
            return disableShowTouches || restoreStayOn != -1 || restoreNormalPowerMode || powerOffScreen;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        byte[] serialize() {
            Parcel parcel = Parcel.obtain();
            writeToParcel(parcel, 0);
            byte[] bytes = parcel.marshall();
            parcel.recycle();
            return bytes;
        }

        static Config deserialize(byte[] bytes) {
            Parcel parcel = Parcel.obtain();
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            return CREATOR.createFromParcel(parcel);
        }

        static Config fromBase64(String base64) {
            byte[] bytes = Base64.decode(base64, Base64.NO_WRAP);
            return deserialize(bytes);
        }

        String toBase64() {
            byte[] bytes = serialize();
            return Base64.encodeToString(bytes, Base64.NO_WRAP);
        }
    }

    private CleanUp() {
        // not instantiable
    }

    public static void configure(int displayId, int restoreStayOn, boolean disableShowTouches, boolean restoreNormalPowerMode, boolean powerOffScreen)
            throws IOException {
        Config config = new Config();
        config.displayId = displayId;
        config.disableShowTouches = disableShowTouches;
        config.restoreStayOn = restoreStayOn;
        config.restoreNormalPowerMode = restoreNormalPowerMode;
        config.powerOffScreen = powerOffScreen;

        if (config.hasWork()) {
            startProcess(config);
        } else {
            // There is no additional clean up to do when scrcpy dies
            unlinkSelf();
        }
    }

    private static void startProcess(Config config) throws IOException {
        String[] cmd = {"app_process", "/", CleanUp.class.getName(), config.toBase64()};

        ProcessBuilder builder = new ProcessBuilder(cmd);
        builder.environment().put("CLASSPATH", SERVER_PATH);
        builder.start();
    }

    private static void unlinkSelf() {
        try {
            new File(SERVER_PATH).delete();
        } catch (Exception e) {
            Ln.e("Could not unlink server", e);
        }
    }

    public static void main(String... args) {
        unlinkSelf();

        try {
            // Wait for the server to die
            System.in.read();
        } catch (IOException e) {
            // Expected when the server is dead
        }

        Ln.i("Cleaning up");

        Config config = Config.fromBase64(args[0]);

        if (config.disableShowTouches || config.restoreStayOn != -1) {
            ServiceManager serviceManager = new ServiceManager();
            Settings settings = new Settings(serviceManager);
            if (config.disableShowTouches) {
                Ln.i("Disabling \"show touches\"");
                try {
                    settings.putValue(Settings.TABLE_SYSTEM, "show_touches", "0");
                } catch (SettingsException e) {
                    Ln.e("Could not restore \"show_touches\"", e);
                }
            }
            if (config.restoreStayOn != -1) {
                Ln.i("Restoring \"stay awake\"");
                try {
                    settings.putValue(Settings.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(config.restoreStayOn));
                } catch (SettingsException e) {
                    Ln.e("Could not restore \"stay_on_while_plugged_in\"", e);
                }
            }
        }

        if (Device.isScreenOn()) {
            if (config.powerOffScreen) {
                Ln.i("Power off screen");
                Device.powerOffScreen(config.displayId);
            } else if (config.restoreNormalPowerMode) {
                Ln.i("Restoring normal power mode");
                Device.setScreenPowerMode(Device.POWER_MODE_NORMAL);
            }
        }
    }
}
