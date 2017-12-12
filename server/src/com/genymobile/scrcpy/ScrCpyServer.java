package com.genymobile.scrcpy;

import java.io.IOException;

public class ScrCpyServer {

    public static void scrcpy() throws IOException {
        ScreenInfo initialScreenInfo = ScreenUtil.getScreenInfo();
        int width = initialScreenInfo.getLogicalWidth();
        int height = initialScreenInfo.getLogicalHeight();
        try (DesktopConnection connection = DesktopConnection.open(width, height)) {
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
