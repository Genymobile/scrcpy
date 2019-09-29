package com.genymobile.scrcpy;

import android.view.MotionEvent;

import java.util.ArrayList;
import java.util.List;

public class FingersState {

    private final List<Finger> fingers = new ArrayList<>();
    private int maxFingers;

    public FingersState(int maxFingers) {
        this.maxFingers = maxFingers;
    }

    private int indexOf(long id) {
        for (int i = 0; i < fingers.size(); ++i) {
            Finger finger = fingers.get(i);
            if (finger.getId() == id) {
                return i;
            }
        }
        return -1;
    }

    private boolean isLocalIdAvailable(int localId) {
        for (int i = 0; i < fingers.size(); ++i) {
            Finger finger = fingers.get(i);
            if (finger.getLocalId() == localId) {
                return false;
            }
        }
        return true;
    }

    private int nextUnusedLocalId() {
        for (int localId = 0; localId < maxFingers; ++localId) {
            if (isLocalIdAvailable(localId)) {
                return localId;
            }
        }
        return -1;
    }

    public Finger get(int index) {
        return fingers.get(index);
    }

    public int getFingerIndex(long id) {
        int index = indexOf(id);
        if (index != -1) {
            // already exists, return it
            return index;
        }
        if (fingers.size() >= maxFingers) {
            // it's full
            return -1;
        }
        // id 0 is reserved for mouse events
        int localId = nextUnusedLocalId();
        if (localId == -1) {
            throw new AssertionError("fingers.size() < maxFingers implies that a local id is available");
        }
        Finger finger = new Finger(id, localId);
        fingers.add(finger);
        // return the index of the finger
        return fingers.size() - 1;
    }

    /**
     * Initialize the motion event parameters.
     *
     * @param props  the pointer properties
     * @param coords the pointer coordinates
     * @return The number of items initialized (the number of fingers).
     */
    public int update(MotionEvent.PointerProperties[] props, MotionEvent.PointerCoords[] coords) {
        for (int i = 0; i < fingers.size(); ++i) {
            Finger finger = fingers.get(i);

            // id 0 is reserved for mouse events
            props[i].id = finger.getLocalId();

            Point point = finger.getPoint();
            coords[i].x = point.getX();
            coords[i].y = point.getY();
            coords[i].pressure = finger.getPressure();
        }
        return fingers.size();
    }

    public void cleanUp() {
        for (int i = fingers.size() - 1; i >= 0; --i) {
            Finger finger = fingers.get(i);
            if (finger.isUp()) {
                fingers.remove(i);
            }
        }
    }
}
