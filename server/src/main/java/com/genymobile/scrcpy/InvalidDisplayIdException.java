package com.genymobile.scrcpy;

public class InvalidDisplayIdException extends RuntimeException {

    private final int displayId;
    private final int[] availableDisplayIds;

    public InvalidDisplayIdException(int displayId, int[] availableDisplayIds) {
        super("There is no display having id " + displayId);
        this.displayId = displayId;
        this.availableDisplayIds = availableDisplayIds;
    }

    public int getDisplayId() {
        return displayId;
    }

    public int[] getAvailableDisplayIds() {
        return availableDisplayIds;
    }
}
