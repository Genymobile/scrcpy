package com.genymobile.scrcpy;

import android.graphics.Bitmap;
import java.nio.ByteBuffer;

public class JpegEncoder {

    static {
        System.loadLibrary("compress");
    }

    public static native byte[] compress(ByteBuffer buffer, int width, int pitch, int height, int quality);
    public static native void test();
}
