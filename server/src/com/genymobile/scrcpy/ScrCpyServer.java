package com.genymobile.scrcpy;

import java.io.IOException;

public class ScrCpyServer {

    private static final String TAG = "scrcpy";

    public static void scrcpy() throws IOException {
        String deviceName = DeviceUtil.getDeviceName();
        ScreenInfo initialScreenInfo = DeviceUtil.getScreenInfo();
        int width = initialScreenInfo.getLogicalWidth();
        int height = initialScreenInfo.getLogicalHeight();
        try (DesktopConnection connection = DesktopConnection.open(deviceName, width, height)) {
            try {
                new ScreenStreamer(connection).streamScreen();
            } catch (IOException e) {
                Ln.e("Screen streaming interrupted", e);
            }
        }
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
