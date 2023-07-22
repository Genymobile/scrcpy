package com.genymobile.scrcpy;

import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;

public enum CameraPosition {
    ALL("all", null),
    FRONT("front", CameraCharacteristics.LENS_FACING_FRONT),
    BACK("back", CameraCharacteristics.LENS_FACING_BACK),
    EXTERNAL("external", CameraCharacteristics.LENS_FACING_EXTERNAL);

    private final String name;
    private final Integer value;

    CameraPosition(String name, Integer value) {
        this.name = name;
        this.value = value;
    }

    String getName() {
        return name;
    }

    boolean matches(String cameraId) {
        if (value == null) {
            return true;
        }
        try {
            CameraCharacteristics cameraCharacteristics = Workarounds.getCameraManager()
                    .getCameraCharacteristics(cameraId);
            Integer lensFacing = cameraCharacteristics.get(CameraCharacteristics.LENS_FACING);
            return value.equals(lensFacing);
        } catch (CameraAccessException e) {
            return false;
        }
    }

    static CameraPosition findByName(String name) {
        for (CameraPosition cameraPosition : CameraPosition.values()) {
            if (name.equals(cameraPosition.name)) {
                return cameraPosition;
            }
        }

        return null;
    }
}
