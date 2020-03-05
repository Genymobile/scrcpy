package com.genymobile.scrcpy;

import android.graphics.Bitmap;
import java.nio.ByteBuffer;

public class JpegEncoder {

    static {
        try {
            System.loadLibrary("compress");
        } catch (UnsatisfiedLinkError e1) {
            try {
                System.load("/data/local/tmp/libturbojpeg.so");
                System.load("/data/local/tmp/libcompress.so");
            } catch (UnsatisfiedLinkError e2) {
                Ln.e("UnsatisfiedLinkError: " + e2.getMessage());
                System.exit(1);
            }
        }
    }

    public static native byte[] compress(ByteBuffer buffer, int width, int pitch, int height, int quality);

    public static native void test();
}
