package com.genymobile.scrcpy.device;

public final class NewDisplay {
    private Size size;
    private int dpi;
    private final boolean resizable;

    public NewDisplay() {
        // Auto size and dpi, not resizable
        this.resizable = false;
    }

    public NewDisplay(Size size, int dpi, boolean resizable) {
        this.size = size;
        this.dpi = dpi;
        this.resizable = resizable;
    }

    public Size getSize() {
        return size;
    }

    public int getDpi() {
        return dpi;
    }

    public boolean isResizable() {
        return resizable;
    }

    public boolean hasExplicitSize() {
        return size != null;
    }

    public boolean hasExplicitDpi() {
        return dpi != 0;
    }

}
