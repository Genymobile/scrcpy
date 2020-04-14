package com.genymobile.scrcpy;

import java.util.HashMap;

public class CodecOptions {
    static final String PROFILE_OPTION = "profile";
    static final String LEVEL_OPTION = "level";

    private HashMap<String, String> options;

    CodecOptions(HashMap<String, String> options) {
        this.options = options;
    }

    Object parseValue(String profileOption) {
        String value = options.get(profileOption);
        switch (profileOption) {
            case PROFILE_OPTION:
            case LEVEL_OPTION:
                return NumberUtils.tryParseInt(value);
            default:
                return null;
        }
    }
}
