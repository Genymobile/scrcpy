package com.genymobile.scrcpy.key2touch;

import com.genymobile.scrcpy.Point;

public final class TouchEvent {

    public final Point point;
    public final boolean repeating;

    public static final int buttons = 1;
    public static final float pressure = 1.0f;
    public static final long pointerId = -2;
    public static final long repeatingInterval = 100;

    public TouchEvent(Point point, boolean repeating)
    {
        this.point = point;
        this.repeating = repeating;

    }
}
