package com.genymobile.scrcpy.device;

public final class NewDisplay {
    private Size size;
    private int dpi;
    private float fps;

    public NewDisplay() {
        // Auto size, dpi and fps
    }

    public NewDisplay(Size size, int dpi, float fps) {
        this.size = size;
        this.dpi = dpi;
        this.fps = fps;
    }

    public Size getSize() {
        return size;
    }

    public int getDpi() {
        return dpi;
    }

    public float getFps() {
        return fps;
    }

    public boolean hasExplicitSize() {
        return size != null;
    }

    public boolean hasExplicitDpi() {
        return dpi != 0;
    }

    public boolean hasExplicitFps() {
        return fps != 0;
    }
}
