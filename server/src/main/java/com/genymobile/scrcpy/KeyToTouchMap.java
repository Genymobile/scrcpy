package com.genymobile.scrcpy;

import android.util.Xml;

import com.genymobile.scrcpy.key2touch.MappingGroup;
import com.genymobile.scrcpy.key2touch.TouchEvent;

import org.xmlpull.v1.XmlPullParser;

import java.io.FileInputStream;
import java.util.ArrayList;

/**
 *
 */
public final class KeyToTouchMap {


    public static final KeyToTouchMap instance = new KeyToTouchMap();


    private final ArrayList<MappingGroup> groupList = new ArrayList<MappingGroup>();
    private int currentGroup = 0;


    private KeyToTouchMap() {
        groupList.add(new MappingGroup("None"));
        currentGroup = 0;
    }

    public void nextGroup()
    {
        ++currentGroup;
        if(currentGroup >= groupList.size())
            currentGroup = 0;

        Ln.i("KeyToTouchMap: Current group " + groupList.get(currentGroup).name);
    }

    public void previousGroup()
    {
        --currentGroup;
        if(currentGroup < 0)
            currentGroup = groupList.size() - 1;

        Ln.i("KeyToTouchMap: Current group " + groupList.get(currentGroup).name);
    }

    public TouchEvent lookup(int keyCode)
    {
        MappingGroup group = groupList.get(currentGroup);
        return group.lookup(keyCode);

    }

    public void parse(String inputFilePath)
    {
        MappingGroup noneGroup = groupList.get(0);
        groupList.clear();
        groupList.add(noneGroup);
        currentGroup = 0;
        try(FileInputStream input = new FileInputStream(inputFilePath))
        {
            XmlPullParser parser = Xml.newPullParser();
            parser.setInput(input, null);
            parser.nextTag();

            while (parser.nextTag() != XmlPullParser.END_TAG) {
                if (parser.getEventType() != XmlPullParser.START_TAG) {
                    continue;
                }
                if(parser.getName().compareTo(MappingGroup.GROUP_TAG) != 0)
                    MappingGroup.skip(parser);

                try {
                    groupList.add(new MappingGroup(parser));
                }
                catch (Exception parsingException)
                {
                    Ln.e("Fail to parse MappingGroup", parsingException);
                    MappingGroup.skip(parser);
                }
            }
        }
        catch (Exception parsingException)
        {
            Ln.e(String.format("Fail to parse %s", inputFilePath), parsingException);
        }

    }

}

