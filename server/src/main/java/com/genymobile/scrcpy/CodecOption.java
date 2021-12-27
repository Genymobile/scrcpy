package com.genymobile.scrcpy;

import java.util.ArrayList;
import java.util.List;

public class CodecOption {
    private String key;
    private Object value;

    public CodecOption(String key, Object value) {
        this.key = key;
        this.value = value;
    }

    public String getKey() {
        return key;
    }

    public Object getValue() {
        return value;
    }

    public static List<CodecOption> parse(String codecOptions) {
        if (codecOptions.isEmpty()) {
            return null;
        }

        List<CodecOption> result = new ArrayList<>();

        boolean escape = false;
        StringBuilder buf = new StringBuilder();

        for (char c : codecOptions.toCharArray()) {
            switch (c) {
                case '\\':
                    if (escape) {
                        buf.append('\\');
                        escape = false;
                    } else {
                        escape = true;
                    }
                    break;
                case ',':
                    if (escape) {
                        buf.append(',');
                        escape = false;
                    } else {
                        // This comma is a separator between codec options
                        String codecOption = buf.toString();
                        result.add(parseOption(codecOption));
                        // Clear buf
                        buf.setLength(0);
                    }
                    break;
                default:
                    buf.append(c);
                    break;
            }
        }

        if (buf.length() > 0) {
            String codecOption = buf.toString();
            result.add(parseOption(codecOption));
        }

        return result;
    }

    private static CodecOption parseOption(String option) {
        int equalSignIndex = option.indexOf('=');
        if (equalSignIndex == -1) {
            throw new IllegalArgumentException("'=' expected");
        }
        String keyAndType = option.substring(0, equalSignIndex);
        if (keyAndType.length() == 0) {
            throw new IllegalArgumentException("Key may not be null");
        }

        String key;
        String type;

        int colonIndex = keyAndType.indexOf(':');
        if (colonIndex != -1) {
            key = keyAndType.substring(0, colonIndex);
            type = keyAndType.substring(colonIndex + 1);
        } else {
            key = keyAndType;
            type = "int"; // assume int by default
        }

        Object value;
        String valueString = option.substring(equalSignIndex + 1);
        switch (type) {
            case "int":
                value = Integer.parseInt(valueString);
                break;
            case "long":
                value = Long.parseLong(valueString);
                break;
            case "float":
                value = Float.parseFloat(valueString);
                break;
            case "string":
                value = valueString;
                break;
            default:
                throw new IllegalArgumentException("Invalid codec option type (int, long, float, str): " + type);
        }

        return new CodecOption(key, value);
    }
}
