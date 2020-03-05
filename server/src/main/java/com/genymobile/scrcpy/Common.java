package com.genymobile.scrcpy;

import android.os.Message;
import android.os.Handler;

public class Common {

    public static void stopScrcpy(Handler handler, String obj) {
        Message msg = Message.obtain();
        msg.what = 1;
        msg.obj = obj;
        try {
            handler.sendMessage(msg);
        } catch (java.lang.IllegalStateException e) {

        }
    }
}