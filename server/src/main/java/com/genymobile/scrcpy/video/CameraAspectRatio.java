package com.genymobile.scrcpy.video;

public final class CameraAspectRatio {
    private static final float SENSOR = -1;

    private float ar;

    private CameraAspectRatio(float ar) {
        this.ar = ar;
    }

    public static CameraAspectRatio fromFloat(float ar) {
        if (ar < 0) {
            throw new IllegalArgumentException("Invalid aspect ratio: " + ar);
        }
        return new CameraAspectRatio(ar);
    }

    public static CameraAspectRatio fromFraction(int w, int h) {
        if (w <= 0 || h <= 0) {
            throw new IllegalArgumentException("Invalid aspect ratio: " + w + ":" + h);
        }
        return new CameraAspectRatio((float) w / h);
    }

    public static CameraAspectRatio sensorAspectRatio() {
        return new CameraAspectRatio(SENSOR);
    }

    public boolean isSensor() {
        return ar == SENSOR;
    }

    public float getAspectRatio() {
        return ar;
    }
}
