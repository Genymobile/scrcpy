package com.genymobile.scrcpy;

import java.io.IOException;

public class ScrCpyServer {

    public static void scrcpy() throws IOException {
        String deviceName = DeviceUtil.getDeviceName();
        ScreenInfo initialScreenInfo = DeviceUtil.getScreenInfo();
        int width = initialScreenInfo.getLogicalWidth();
        int height = initialScreenInfo.getLogicalHeight();
        try (DesktopConnection connection = DesktopConnection.open(deviceName, width, height)) {
            try {
                new ScreenStreamer(connection).streamScreen();
            } catch (IOException e) {
                System.err.println("Screen streaming interrupted: " + e.getMessage());
                System.err.flush();
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
