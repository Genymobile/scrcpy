package com.genymobile.scrcpy.key2touch;

import com.genymobile.scrcpy.Ln;
import com.genymobile.scrcpy.Point;

import org.xmlpull.v1.XmlPullParser;

import java.util.HashMap;

public final class MappingGroup {

    public static final String GROUP_TAG = "Group";
    private static final String MAPPING_TAG = "Mapping";
    private static final String NAME_ATTR = "name";
    private static final String KEY_ATTR = "key";
    private static final String X_ATTR = "x";
    private static final String Y_ATTR = "y";
    private static final String REPEATING_ATTR = "repeating";


    public final String name;
    private HashMap<Integer, TouchEvent> map = new HashMap<Integer, TouchEvent>();

    private boolean parseMapping(XmlPullParser parser)
    {
        if (parser.getName().compareTo(MAPPING_TAG) != 0) {
            return false;
        }

        try {
            String keyStr = parser.getAttributeValue(null, KEY_ATTR);
            int key = -1;
            if (keyStr != null && keyStr.toLowerCase().startsWith("\\u")) {
                key = Integer.valueOf(keyStr.substring(2), 16);
            } else
                throw new NumberFormatException("The format is invalid: " + keyStr);


            int x = Integer.valueOf(parser.getAttributeValue(null, X_ATTR));
            int y = Integer.valueOf(parser.getAttributeValue(null, Y_ATTR));
            boolean repeating = Boolean.parseBoolean(parser.getAttributeValue(null, REPEATING_ATTR));
            Ln.d("key = " + Character.toChars(key)[0] + ", x = " + x + ", y = " + y + ", repeating = " + repeating);
            map.put(key, new TouchEvent(new Point(x, y), repeating));
            parser.nextTag();

        }
        catch (Exception parsingException)
        {
            Ln.e(String.format("Error while parsing %s", this.name), parsingException);
            return false;
        }
        return true;
    }

    public MappingGroup(String name)
    {
        this.name = name;
    }

    public MappingGroup(XmlPullParser parser) throws Exception
    {
        name = parser.getAttributeValue(null, NAME_ATTR);
        while (parser.nextTag() != XmlPullParser.END_TAG) {
            if (parser.getEventType() != XmlPullParser.START_TAG) {
                continue;
            }
            if(!parseMapping(parser))
                skip(parser);

        }

    }



    public TouchEvent lookup(int keyCode)
    {
        return map.get(keyCode);
    }



    public static void skip(XmlPullParser parser) throws Exception
    {

        int depth = 1;
        while (depth != 0) {
            switch (parser.next()) {
                case XmlPullParser.END_TAG:
                    Ln.w(String.format("MappingGroup skipping: %s", parser.getName()));
                    depth--;
                    break;
                case XmlPullParser.START_TAG:
                    depth++;
                    break;
            }
        }

    }
}
