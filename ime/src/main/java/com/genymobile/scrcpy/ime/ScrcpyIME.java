package com.genymobile.scrcpy.ime;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.inputmethodservice.InputMethodService;
import android.util.Log;
import android.view.KeyEvent;
import android.view.inputmethod.InputConnection;

public class ScrcpyIME extends InputMethodService {
    private static String TAG = "ScrcpyIME";
    private BroadcastReceiver receiver;
    private static final String COMMIT_TEXT_ACTION = "com.genymobile.scrcpy.ime.COMMIT_TEXT_ACTION";
    private static final String STATE_CHANGE_ACTION = "com.genymobile.scrcpy.ime.STATE_CHANGE_ACTION";

    @Override
    public void onCreate() {
        super.onCreate();
        this.receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                InputConnection inputConnection = getCurrentInputConnection();
                if(inputConnection == null){
                    return;
                }
                String text = null;
                KeyEvent keyEvent = null;
                if((text = intent.getStringExtra("text")) != null && text.length() > 0) {
                   inputConnection.commitText(text, 0);
                }else if((keyEvent = intent.getParcelableExtra("keyEvent")) != null) {
                    inputConnection.sendKeyEvent(keyEvent);
                }
            }
        };
        IntentFilter localIntentFilter = new IntentFilter(COMMIT_TEXT_ACTION);
        registerReceiver(this.receiver, localIntentFilter);
    }

    @Override
    public void onDestroy() {
        unregisterReceiver(this.receiver);
        Log.i(TAG, "disabling self due to destroy");
        super.onDestroy();
    }

    @Override
    public void onBindInput() {
        super.onBindInput();
        Log.i(TAG, "BindInput");
        sendStateBroadcast("BindInput");
    }

    @Override
    public void onUnbindInput() {
        super.onUnbindInput();
        Log.i(TAG, "UnbindInput");
        sendStateBroadcast("UnbindInput");
    }

    @Override
    public void onWindowShown() {
        super.onWindowShown();
        Log.i(TAG, "WindowShown");
        sendStateBroadcast("WindowShown");
    }

    @Override
    public void onWindowHidden() {
        super.onWindowHidden();
        Log.i(TAG, "WindowHidden");
        sendStateBroadcast("WindowHidden");
    }

    private void sendStateBroadcast(String state) {
        Intent intent = new Intent(STATE_CHANGE_ACTION);
        intent.putExtra("state", state);
        sendBroadcast(intent);
    }
}
