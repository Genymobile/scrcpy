package com.genymobile.scrcpy;

import android.media.MediaCodecInfo;
import android.os.Build;

import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;

public class CodecOptions {

    public static final String PROFILE_OPTION = "profile";
    public static final String LEVEL_OPTION = "level";

    private static final LinkedHashMap<Integer, String> levelsTable = new LinkedHashMap<Integer, String>() {
        {
            // Adding all possible level and their properties
            // 3rd property, bitrate was added but not sure if needed for now.
            // Source: https://en.wikipedia.org/wiki/Advanced_Video_Coding#Levels
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel1, "485,99,64");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel1b, "485,99,128");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel11, "3000,396,192");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel12, "6000,396,384");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel13, "11880,396,768");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel2, "11880,396,2000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel21, "19800,792,4000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel22, "20250,1620,4000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel3, "40500,1620,10000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel31, "108000,3600,14000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel32, "216000,5120,20000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel4, "245760,8192,20000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel41, "245760,8192,50000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel42, "522240,8704,50000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel5, "589824,22080,135000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel51, "983040,36864,240000");
            put(MediaCodecInfo.CodecProfileLevel.AVCLevel52, "2073600,36864,240000");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                put(MediaCodecInfo.CodecProfileLevel.AVCLevel6, "4177920,139264,240000");
                put(MediaCodecInfo.CodecProfileLevel.AVCLevel61, "8355840,139264,480000");
                put(MediaCodecInfo.CodecProfileLevel.AVCLevel62, "16711680,139264,800000");
            }
        }
    };

    private HashMap<String, String> options;

    CodecOptions(HashMap<String, String> options) {
        this.options = options;
    }

    /**
     * The purpose of this function is to return the lowest possible codec profile level
     * that supports the given width/height/bitrate of the stream
     * @param width of the device
     * @param height of the device
     * @param bitRate at which we stream
     * @return the lowest possible level that should support the given properties.
     */
    public static int calculateLevel(int width, int height, int bitRate) {
        // Calculations source: https://stackoverflow.com/questions/32100635/vlc-2-2-and-levels
        int macroblocks = (int)( Math.ceil(width/16.0) * Math.ceil(height/16.0) );
        int macroblocks_s = macroblocks * 60;
        for (Map.Entry<Integer, String> entry : levelsTable.entrySet()) {
            String[] levelProperties = entry.getValue().split(",");
            int levelMacroblocks_s = Integer.parseInt(levelProperties[0]);
            int levelMacroblocks = Integer.parseInt(levelProperties[1]);
            if(macroblocks_s > levelMacroblocks_s) continue;
            if(macroblocks > levelMacroblocks) continue;
            Ln.i("Level selected based on screen size calculation: " + entry.getKey());
            return entry.getKey();
        }
        return 0;
    }

    public Object parseValue(String profileOption) {
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
