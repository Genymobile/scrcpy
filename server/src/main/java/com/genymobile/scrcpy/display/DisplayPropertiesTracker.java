package com.genymobile.scrcpy.display;

import com.genymobile.scrcpy.util.Ln;

public final class DisplayPropertiesTracker {
    private DisplayProperties props;

    public synchronized void setCurrent(DisplayProperties props) {
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

        if (props.equals(prev)) {
            if (Ln.isEnabled(Ln.Level.VERBOSE)) {
                Ln.v(getClass().getSimpleName() + ": " + props + " (unchanged)");
            }
            return false;
        }

        if (Ln.isEnabled(Ln.Level.VERBOSE)) {
            Ln.v(getClass().getSimpleName() + ": "  + prev + " -> " + props);
        }

        return true;
    }
}
