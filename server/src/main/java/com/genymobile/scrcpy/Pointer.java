package com.genymobile.scrcpy;

public class Pointer {

    /**
     * Pointer id as received from the client.
     */
    private final long id;

    /**
     * Local pointer id, using the lowest possible values to fill the {@link android.view.MotionEvent.PointerProperties PointerProperties}.
     */
    private final int localId;

    private Point point;
    private float pressure;
    private boolean up;

    public Pointer(long id, int localId) {
        this.id = id;
        this.localId = localId;
    }

    public long getId() {
        return id;
    }

    public int getLocalId() {
        return localId;
    }

    public Point getPoint() {
        return point;
    }

    public void setPoint(Point point) {
        this.point = point;
    }

    public float getPressure() {
        return pressure;
    }

    public void setPressure(float pressure) {
        this.pressure = pressure;
    }

    public boolean isUp() {
        return up;
    }

    public void setUp(boolean up) {
        this.up = up;
    }
}
