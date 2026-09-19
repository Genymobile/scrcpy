package com.genymobile.scrcpy.display;

import com.genymobile.scrcpy.model.Size;

public final class DisplayInfo {
    private final int displayId;
    private final Size size;
    private final int rotation;
    private final int layerStack;
    private final int flags;
    private final int dpi;
    private final String uniqueId;
    private final float refreshRate;

    public static final int FLAG_SUPPORTS_PROTECTED_BUFFERS = 0x00000001;

    @SuppressWarnings("checkstyle:ParameterNumber")
    public DisplayInfo(int displayId, Size size, int rotation, int layerStack, int flags, int dpi, String uniqueId, float refreshRate) {
        this.displayId = displayId;
        this.size = size;
        this.rotation = rotation;
        this.layerStack = layerStack;
        this.flags = flags;
        this.dpi = dpi;
        this.uniqueId = uniqueId;
        this.refreshRate = refreshRate;
    }

    public int getDisplayId() {
        return displayId;
    }

    public Size getSize() {
        return size;
    }

    public int getRotation() {
        return rotation;
    }

    public int getLayerStack() {
        return layerStack;
    }

    public int getFlags() {
        return flags;
    }

    public int getDpi() {
        return dpi;
    }

    public String getUniqueId() {
        return uniqueId;
    }

    public float getRefreshRate() {
        return refreshRate;
    }
}
