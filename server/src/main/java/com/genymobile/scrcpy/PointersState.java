package com.genymobile.scrcpy;

import android.view.MotionEvent;

import java.util.ArrayList;
import java.util.List;

public class PointersState {

    public static final int MAX_POINTERS = 10;

    private final List<Pointer> pointers = new ArrayList<>();

    private int indexOf(long id) {
        for (int i = 0; i < pointers.size(); ++i) {
            Pointer pointer = pointers.get(i);
            if (pointer.getId() == id) {
                return i;
            }
        }
        return -1;
    }

    private boolean isLocalIdAvailable(int localId) {
        for (int i = 0; i < pointers.size(); ++i) {
            Pointer pointer = pointers.get(i);
            if (pointer.getLocalId() == localId) {
                return false;
            }
        }
        return true;
    }

    private int nextUnusedLocalId() {
        for (int localId = 0; localId < MAX_POINTERS; ++localId) {
            if (isLocalIdAvailable(localId)) {
                return localId;
            }
        }
        return -1;
    }

    public Pointer get(int index) {
        return pointers.get(index);
    }

    public int getPointerIndex(long id) {
        int index = indexOf(id);
        if (index != -1) {
            // already exists, return it
            return index;
        }
        if (pointers.size() >= MAX_POINTERS) {
            // it's full
            return -1;
        }
        // id 0 is reserved for mouse events
        int localId = nextUnusedLocalId();
        if (localId == -1) {
            throw new AssertionError("pointers.size() < maxFingers implies that a local id is available");
        }
        Pointer pointer = new Pointer(id, localId);
        pointers.add(pointer);
        // return the index of the pointer
        return pointers.size() - 1;
    }

    /**
     * Initialize the motion event parameters.
     *
     * @param props  the pointer properties
     * @param coords the pointer coordinates
     * @return The number of items initialized (the number of pointers).
     */
    public int update(MotionEvent.PointerProperties[] props, MotionEvent.PointerCoords[] coords) {
        int count = pointers.size();
        for (int i = 0; i < count; ++i) {
            Pointer pointer = pointers.get(i);

            // id 0 is reserved for mouse events
            props[i].id = pointer.getLocalId();

            Point point = pointer.getPoint();
            coords[i].x = point.getX();
            coords[i].y = point.getY();
            coords[i].pressure = pointer.getPressure();
        }
        cleanUp();
        return count;
    }

    /**
     * Remove all pointers which are UP.
     */
    private void cleanUp() {
        for (int i = pointers.size() - 1; i >= 0; --i) {
            Pointer pointer = pointers.get(i);
            if (pointer.isUp()) {
                pointers.remove(i);
            }
        }
    }
}
