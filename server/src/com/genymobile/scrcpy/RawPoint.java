package com.genymobile.scrcpy;

public class RawPoint {
    private int x;
    private int y;

    public RawPoint(int x, int y) {
        this.x = x;
        this.y = y;
    }

    public int getX() {
        return x;
    }

    public int getY() {
        return y;
    }

    @Override
    public String toString() {
        return "RawPoint{" +
                "x=" + x +
                ", y=" + y +
                '}';
    }
}
