package com.genymobile.scrcpy.device;

public enum Orientation {

    // @formatter:off
    Orient0("0"),
    Orient90("90"),
    Orient180("180"),
    Orient270("270"),
    Flip0("flip0"),
    Flip90("flip90"),
    Flip180("flip180"),
    Flip270("flip270");

    public enum Lock {
        Unlocked, LockedInitial, LockedValue,
    }

    private final String name;

    Orientation(String name) {
        this.name = name;
    }

    public static Orientation getByName(String name) {
        for (Orientation orientation : values()) {
            if (orientation.name.equals(name)) {
                return orientation;
            }
        }

        throw new IllegalArgumentException("Unknown orientation: " + name);
    }

    public static Orientation fromRotation(int ccwRotation) {
        assert ccwRotation >= 0 && ccwRotation < 4;
        // Display rotation is expressed counter-clockwise, orientation is expressed clockwise
        int cwRotation = (4 - ccwRotation) % 4;
        return values()[cwRotation];
    }

    public boolean isFlipped() {
        return (ordinal() & 4) != 0;
    }

    public int getRotation() {
        return ordinal() & 3;
    }
}
