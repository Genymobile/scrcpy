package com.genymobile.scrcpy.video;

import android.annotation.SuppressLint;
import android.hardware.camera2.CameraCharacteristics;

import androidx.annotation.Nullable;

@SuppressLint("InlinedApi")
public enum CameraFacing {
    FRONT("front", CameraCharacteristics.LENS_FACING_FRONT),
    BACK("back", CameraCharacteristics.LENS_FACING_BACK),
    EXTERNAL("external", CameraCharacteristics.LENS_FACING_EXTERNAL);

    private final String name;
    private final int value;

    CameraFacing(String name, int value) {
        this.name = name;
        this.value = value;
    }

    int value() {
        return value;
    }

    @Nullable
    public static CameraFacing findByName(String name) {
        for (CameraFacing facing : CameraFacing.values()) {
            if (name.equals(facing.name)) {
                return facing;
            }
        }

        return null;
    }
}
