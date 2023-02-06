package com.genymobile.scrcpy;

public interface Codec {

    enum Type {
        VIDEO,
    }

    Type getType();

    int getId();

    String getName();

    String getMimeType();
}
