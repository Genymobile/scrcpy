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

    public Finger get(long id) {
        int index = indexOf(id);
        if (index != -1) {
            // already exists, return it
            return fingers[index];
        }
        index = indexOfFirstEmpty();
        if (index == -1) {
            // it's full
            return null;
        }
        // id 0 is reserved for mouse events
        int localId = index;// + 1;
        Finger finger = new Finger(id, localId);
        fingers[index] = finger;
        return finger;
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
                props[count].id = finger.getLocalId();
                Ln.d("update id = " + finger.getLocalId());

                Point point = finger.getPoint();
                coords[count].x = point.getX();
                coords[count].y = point.getY();
                coords[count].pressure = finger.getPressure();

                if (finger.isUp()) {
                    // remove it
                    fingers[i] = null;
                }

                ++count;
            }
        }
        return count;
    }
}
