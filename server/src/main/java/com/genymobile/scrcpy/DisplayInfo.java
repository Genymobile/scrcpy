package com.genymobile.scrcpy;

public final class DisplayInfo {
    private final int displayId;
    private final Size size;
    private final int rotation;
    private final int layerStack;
    private final int flags;

    public static final int DEFAULT_DISPLAY = 0x00000000;

    public static final int FLAG_PRESENTATION = 0x00000008;

    public static final int FLAG_SUPPORTS_PROTECTED_BUFFERS = 0x00000001;

    public DisplayInfo(int displayId, Size size, int rotation, int layerStack, int flags) {
        this.displayId = displayId;
        this.size = size;
        this.rotation = rotation;
        this.layerStack = layerStack;
        this.flags = flags;
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

    public boolean isPresentation() {
        return (flags & FLAG_PRESENTATION) == FLAG_PRESENTATION;
    }

    public boolean isSupportProtectedBuffers() {
        return (flags & FLAG_SUPPORTS_PROTECTED_BUFFERS) == FLAG_SUPPORTS_PROTECTED_BUFFERS;
    }
}

