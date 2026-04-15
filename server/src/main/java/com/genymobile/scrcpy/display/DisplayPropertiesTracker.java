package com.genymobile.scrcpy.display;

import com.genymobile.scrcpy.util.Ln;

import java.util.ArrayList;
import java.util.List;

public final class DisplayPropertiesTracker {

    private static class PendingChange {
        private final DisplayProperties props;
        private final long timestamp;

        PendingChange(DisplayProperties props, long timestamp) {
            this.props = props;
            this.timestamp = timestamp;
        }
    }

    private static final long PENDING_CACHE_DURATION = 2000; // ms
    private final List<PendingChange> pending = new ArrayList<>();

    private DisplayProperties props;

    private static long nowMs() {
        return System.nanoTime() / 1000000;
    }

    public synchronized void expectChange(DisplayProperties props) {
        long now = nowMs();
        pending.add(new PendingChange(props, now));
        this.props = props;
    }

    /**
     * Set the current display properties, and indicate whether the capture must be reset.
     *
     * @param props the current display properties
     * @return {@code true} if the capture must be reset
     */
    public synchronized boolean onDisplayPropertiesChanged(DisplayProperties props) {
        DisplayProperties prev = this.props;
        this.props = props;

        if (props == null) {
            // The display properties have changed, but are unknown, a resize event must be triggered
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v(getClass().getSimpleName() + ": " + prev + " -> (unknown)");
            }
            return true;
        }

        if (consumeExpectedChange(props)) {
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v(getClass().getSimpleName() + ": " + prev + " -> " + props + " (ignored)");
            }
            return false;
        }

        if (props.equals(prev)) {
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v(getClass().getSimpleName() + ": " + props + "(unchanged)");
            }
            return false;
        }

        if (Ln.isEnabled(Ln.Level.VERBOSE)) {
            Ln.v(getClass().getSimpleName() + ": "  + prev + " -> " + props);
        }

        return true;
    }

    private boolean consumeExpectedChange(DisplayProperties props) {
        cleanExpired();
        int index = getMatchingPendingIndex(props);
        if (index == -1) {
            return false;
        }

        // Remove all pending changes up to (and including) the matching one
        pending.subList(0, index+1).clear();
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
        long now = nowMs();
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
