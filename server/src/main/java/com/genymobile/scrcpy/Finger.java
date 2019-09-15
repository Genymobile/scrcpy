package com.genymobile.scrcpy;

public class Finger {

    private final long id;
    private Point point;
    private float pressure;
    private boolean up;

    public Finger(long id) {
        this.id = id;
    }

    public long getId() {
        return id;
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
