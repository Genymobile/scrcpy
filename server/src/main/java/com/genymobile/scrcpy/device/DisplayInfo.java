package com.genymobile.scrcpy.device;

import com.genymobile.scrcpy.AndroidVersions;

import android.os.Build;
import android.os.Process;

public final class DisplayInfo {
    private final int displayId;
    private final Size size;
    private final int rotation;
    private final int layerStack;
    private final int flags;
    private final int dpi;
    private final int type;
    private final int ownerUid;

    public static final int FLAG_SUPPORTS_PROTECTED_BUFFERS = 1 << 0;
    public static final int FLAG_SECURE = 1 << 1;
    public static final int FLAG_PRIVATE = 1 << 2;
    public static final int FLAG_PRESENTATION = 1 << 3;
    public static final int FLAG_ROUND = 1 << 4;
    public static final int FLAG_CAN_SHOW_WITH_INSECURE_KEYGUARD = 1 << 5;
    public static final int FLAG_SHOULD_SHOW_SYSTEM_DECORATIONS = 1 << 6;
    public static final int FLAG_TRUSTED = 1 << 7;
    public static final int FLAG_OWN_DISPLAY_GROUP = 1 << 8;
    public static final int FLAG_ALWAYS_UNLOCKED = 1 << 9;
    public static final int FLAG_TOUCH_FEEDBACK_DISABLED = 1 << 10;
    public static final int FLAG_OWN_FOCUS = 1 << 11;
    public static final int FLAG_STEAL_TOP_FOCUS_DISABLED = 1 << 12;
    public static final int FLAG_REAR = 1 << 13;
    public static final int FLAG_ROTATES_WITH_CONTENT = 1 << 14;
    public static final int FLAG_SCALING_DISABLED = 1 << 30;

    public static final int TYPE_UNKNOWN = 0;
    public static final int TYPE_BUILT_IN = 1;
    public static final int TYPE_INTERNAL = TYPE_BUILT_IN;
    public static final int TYPE_HDMI = 2;
    public static final int TYPE_EXTERNAL = TYPE_HDMI;
    public static final int TYPE_WIFI = 3;
    public static final int TYPE_OVERLAY = 4;
    public static final int TYPE_VIRTUAL = 5;

    @SuppressWarnings("checkstyle:ParameterNumber")
    public DisplayInfo(int displayId, Size size, int rotation, int layerStack, int flags, int dpi, int type, int ownerUid) {
        this.displayId = displayId;
        this.size = size;
        this.rotation = rotation;
        this.layerStack = layerStack;
        this.flags = flags;
        this.dpi = dpi;
        this.type = type;
        this.ownerUid = ownerUid;
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

    public int getType() {
        return type;
    }

    public int getOwnerUid() {
        return ownerUid;
    }

    public boolean isTrusted() {
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_30_ANDROID_11) {
            return (flags & FLAG_TRUSTED) == FLAG_TRUSTED;
        } else if (Build.VERSION.SDK_INT == AndroidVersions.API_29_ANDROID_10) {
            return type != TYPE_VIRTUAL || ownerUid == Process.SYSTEM_UID;
        }
        return true;
    }
}

