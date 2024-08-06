package com.genymobile.scrcpy;

import com.genymobile.scrcpy.device.Size;

import java.util.Objects;

public final class DisplaySpec {
    private final Size size;
    private final int dpi;

    public DisplaySpec(Size size, int dpi) {
        this.size = size;
        this.dpi = dpi;
    }

    public Size getSize() {
        return size;
    }

    public int getDpi() {
        return dpi;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        DisplaySpec spec = (DisplaySpec) o;
        return size.equals(spec.size) && dpi == spec.dpi;
    }

    @Override
    public int hashCode() {
        return Objects.hash(size.hashCode(), dpi);
    }

    @Override
    public String toString() {
        return "DisplaySpec{" + "size=" + size + ", dpi=" + dpi + '}';
    }
}
