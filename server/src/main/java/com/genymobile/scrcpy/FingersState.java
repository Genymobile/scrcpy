package com.genymobile.scrcpy;

import android.view.MotionEvent;

public class FingersState {

    /**
     * Array of enabled fingers (can contain "null" holes).
     * <p>
     * Once a Finger (identified by its id received from the client) is enabled, it is never moved.
     * <p>
     * Its index is its local identifier injected into MotionEvents.
     */
    private final Finger[] fingers;

    public FingersState(int maxFingers) {
        fingers = new Finger[maxFingers];
    }

    private int indexOf(long id) {
        for (int i = 0; i < fingers.length; ++i) {
            Finger finger = fingers[i];
            if (finger != null && finger.getId() == id) {
                return i;
            }
        }
        return -1;
    }

    private int indexOfFirstEmpty() {
        for (int i = 0; i < fingers.length; ++i) {
            if (fingers[i] == null) {
                return i;
            }
        }
        return -1;
    }

    private Finger create(long id) {
        int index = indexOf(id);
        if (index != -1) {
            // already exists, return it
            return fingers[index];
        }
        int firstEmpty = indexOfFirstEmpty();
        if (firstEmpty == -1) {
            // it's full
            return null;
        }
        Finger finger = new Finger(id);
        fingers[firstEmpty] = finger;
        return finger;
    }

    public boolean unset(long id) {
        int index = indexOf(id);
        if (index == -1) {
            return false;
        }
        fingers[index] = null;
        return true;
    }

    public boolean set(long id, Point point, float pressure, boolean up) {
        Finger finger = create(id);
        if (finger == null) {
            return false;
        }
        finger.setPoint(point);
        finger.setPressure(pressure);
        finger.setUp(up);
        return true;
    }

    public void cleanUp() {
        for (int i = 0; i < fingers.length; ++i) {
            if (fingers[i] != null && fingers[i].isUp()) {
                fingers[i] = null;
            }
        }
    }

    /**
     * Initialize the motion event parameters.
     *
     * @param props  the pointer properties
     * @param coords the pointer coordinates
     * @return The number of items initialized (the number of fingers).
     */
    public int update(MotionEvent.PointerProperties[] props, MotionEvent.PointerCoords[] coords) {
        int count = 0;
        for (int i = 0; i < fingers.length; ++i) {
            Finger finger = fingers[i];
            if (finger != null) {
                // id 0 is reserved for mouse events
                props[count].id = i + 1;

                Point point = finger.getPoint();
                coords[i].x = point.getX();
                coords[i].y = point.getY();
                coords[i].pressure = finger.getPressure();

                ++count;
            }
        }
        return count;
    }
}
