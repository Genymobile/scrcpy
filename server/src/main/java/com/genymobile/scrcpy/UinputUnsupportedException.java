package com.genymobile.scrcpy;

public class UinputUnsupportedException extends RuntimeException {
    public UinputUnsupportedException(Exception e) {
        super("device does not support uinput without root", e);
    }
}
