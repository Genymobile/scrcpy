package com.genymobile.scrcpy;

import com.genymobile.scrcpy.wrappers.ContentProvider;

import android.os.BatteryManager;
import android.os.Build;

import java.io.IOException;
import java.nio.ByteBuffer;

public abstract class Connection implements Device.RotationListener, Device.ClipboardListener {
    public interface StreamInvalidateListener {
        void onStreamInvalidate();
    }

    protected final ControlMessageReader reader = new ControlMessageReader();
    protected static final int DEVICE_NAME_FIELD_LENGTH = 64;
    protected StreamInvalidateListener streamInvalidateListener;
    protected Device device;
    protected final VideoSettings videoSettings;
    protected final Options options;
    protected Controller controller;
    protected ScreenEncoder screenEncoder;

    abstract void send(ByteBuffer data);

    abstract void sendDeviceMessage(DeviceMessage msg) throws IOException;

    abstract void close() throws Exception;

    abstract boolean hasConnections();

    public Connection(Options options, VideoSettings videoSettings) {
        Ln.i("Device: " + Build.MANUFACTURER + " " + Build.MODEL + " (Android " + Build.VERSION.RELEASE + ")");
        this.videoSettings = videoSettings;
        this.options = options;
        device = new Device(options, videoSettings);
        device.setRotationListener(this);
        controller = new Controller(device, this);
        startDeviceMessageSender(controller.getSender());
        device.setClipboardListener(this);

        boolean mustDisableShowTouchesOnCleanUp = false;
        int restoreStayOn = -1;
        if (options.getShowTouches() || options.getStayAwake()) {
            try (ContentProvider settings = Device.createSettingsProvider()) {
                if (options.getShowTouches()) {
                    String oldValue = settings.getAndPutValue(ContentProvider.TABLE_SYSTEM, "show_touches", "1");
                    // If "show touches" was disabled, it must be disabled back on clean up
                    mustDisableShowTouchesOnCleanUp = !"1".equals(oldValue);
                }

                if (options.getStayAwake()) {
                    int stayOn = BatteryManager.BATTERY_PLUGGED_AC | BatteryManager.BATTERY_PLUGGED_USB | BatteryManager.BATTERY_PLUGGED_WIRELESS;
                    String oldValue = settings.getAndPutValue(ContentProvider.TABLE_GLOBAL, "stay_on_while_plugged_in", String.valueOf(stayOn));
                    try {
                        restoreStayOn = Integer.parseInt(oldValue);
                        if (restoreStayOn == stayOn) {
                            // No need to restore
                            restoreStayOn = -1;
                        }
                    } catch (NumberFormatException e) {
                        restoreStayOn = 0;
                    }
                }
            }
        }

        try {
            CleanUp.configure(options.getDisplayId(), restoreStayOn, mustDisableShowTouchesOnCleanUp, true, options.getPowerOffScreenOnClose());
        } catch (IOException e) {
            Ln.w("CleanUp.configure() failed:" + e.getMessage());
        }
    }

    public boolean setVideoSettings(VideoSettings newSettings) {
        if (!videoSettings.equals(newSettings)) {
            videoSettings.merge(newSettings);
            device.applyNewVideoSetting(videoSettings);
            if (this.streamInvalidateListener != null) {
                streamInvalidateListener.onStreamInvalidate();
            }
            return true;
        }
        return false;
    }

    public void setStreamInvalidateListener(StreamInvalidateListener listener) {
        this.streamInvalidateListener = listener;
    }

    @Override
    public void onRotationChanged(int rotation) {
        if (this.streamInvalidateListener != null) {
            streamInvalidateListener.onStreamInvalidate();
        }
    }

    @Override
    public void onClipboardTextChanged(String text) {
        controller.getSender().pushClipboardText(text);
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
}
