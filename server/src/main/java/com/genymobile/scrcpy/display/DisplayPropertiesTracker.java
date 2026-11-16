package com.genymobile.scrcpy.display;

import android.os.SystemClock;

import java.util.ArrayList;
import java.util.List;

public class DisplayPropertiesTracker {

    private static final long PENDING_CACHE_DURATION = 3000; // ms

    private static class PendingChange {
        private final DisplayProperties props;
        private final long timestamp;

        PendingChange(DisplayProperties props, long timestamp) {
            this.props = props;
            this.timestamp = timestamp;
        }
    }

    private final List<PendingChange> pending = new ArrayList<>();

    public synchronized void pushClientRequest(DisplayProperties props) {
        long now = SystemClock.uptimeMillis();
        pending.add(new PendingChange(props, now));
    }

    /**
     * Function to be called when the display properties changed.
     *
     * @param props the new display properties
     * @return {@code true} if this change is the result of a client request
     */
    public synchronized boolean onChanged(DisplayProperties props) {
        cleanExpired();
        int index = getMatchingPendingIndex(props);
        if (index == -1) {
            return false;
        }

        pending.subList(0, index + 1).clear();
        return true;
    }

    private int getMatchingPendingIndex(DisplayProperties props) {
        for (int i = 0; i < pending.size(); ++i) {
            if (pending.get(i).props.equals(props)) {
                return i;
            }
        }
        return -1;
    }

    private int getFirstNonExpiredIndex() {
        long now = SystemClock.uptimeMillis();
        for (int i = 0; i < pending.size(); ++i) {
            if (pending.get(i).timestamp + PENDING_CACHE_DURATION >= now) {
                return i;
            }
        }
        return -1;
    }

    private void cleanExpired() {
        if (pending.isEmpty()) {
            return;
        }

        int firstNonExpiredIndex = getFirstNonExpiredIndex();
        if (firstNonExpiredIndex == 0) {
            // All items are fresh
            return;
        }

        if (firstNonExpiredIndex == -1) {
            // All items have expired
            pending.clear();
        } else {
            // Remove all the items up to the first non-expired index
            pending.subList(0, firstNonExpiredIndex).clear();
        }
    }
}
