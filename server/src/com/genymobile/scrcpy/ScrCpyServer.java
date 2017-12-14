package com.genymobile.scrcpy;

import java.io.IOException;

public class ScrCpyServer {

    private static final String TAG = "scrcpy";

    private static void scrcpy() throws IOException {
        String deviceName = DeviceUtil.getDeviceName();
        ScreenInfo initialScreenInfo = DeviceUtil.getScreenInfo();
        int width = initialScreenInfo.getLogicalWidth();
        int height = initialScreenInfo.getLogicalHeight();
        try (DesktopConnection connection = DesktopConnection.open(deviceName, width, height)) {
            try {
                // asynchronous
                startEventController(connection);
                // synchronous
                new ScreenStreamer(connection).streamScreen();
            } catch (IOException e) {
                Ln.e("Screen streaming interrupted", e);
            }
        }
    }

    private static void startEventController(final DesktopConnection connection) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    new EventController(connection).control();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }).start();
    }

    public static void main(String... args) throws Exception {
        try {
            scrcpy();
        } catch (Throwable t) {
            t.printStackTrace();
            throw t;
        }
    }
}
