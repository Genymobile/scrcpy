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
    private final int type;
    private final String name;

    public static final int FLAG_SUPPORTS_PROTECTED_BUFFERS = 0x00000001;

    // android.view.Display.TYPE_*
    public static final int TYPE_UNKNOWN = 0;
    public static final int TYPE_INTERNAL = 1;
    public static final int TYPE_EXTERNAL = 2;
    public static final int TYPE_WIFI = 3;
    public static final int TYPE_OVERLAY = 4;
    public static final int TYPE_VIRTUAL = 5;

    @SuppressWarnings("checkstyle:ParameterNumber")
    public DisplayInfo(int displayId, Size size, int rotation, int layerStack, int flags, int dpi, String uniqueId, int type, String name) {
        this.displayId = displayId;
        this.size = size;
        this.rotation = rotation;
        this.layerStack = layerStack;
        this.flags = flags;
        this.dpi = dpi;
        this.uniqueId = uniqueId;
        this.type = type;
        this.name = name;
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

    public int getType() {
        return type;
    }

    public String getName() {
        return name;
    }
}
