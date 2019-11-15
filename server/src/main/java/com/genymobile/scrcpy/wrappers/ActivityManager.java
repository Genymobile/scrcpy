package com.genymobile.scrcpy.wrappers;

import java.lang.reflect.Method;

import android.annotation.SuppressLint;
import android.content.IIntentReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.IInterface;
import android.os.RemoteException;
import com.genymobile.scrcpy.Ln;

@SuppressLint("PrivateApi")
public class ActivityManager {
    private final IInterface manager;
    private Method broadcastIntentMethod;
    private Method registerReceiverMethod;

    public ActivityManager(IInterface manager) {
        this.manager = manager;
        try {
            for (Method method : manager.getClass().getDeclaredMethods()) {
                if (method.getName().equals("broadcastIntent")) {
                    int parameterLength = method.getParameterTypes().length;
                    if (parameterLength != 13 && parameterLength != 12 && parameterLength != 11) {
                        Ln.i("broadcastIntent method parameter length wrong.");
                        continue;
                    }
                    broadcastIntentMethod = method;
                }else if(method.getName().equals("registerReceiver")) {
                    registerReceiverMethod = method;
                }
            }
        } catch (Exception e) {
            throw new AssertionError(e);
        }

    }

    public void sendBroadcast(Intent paramIntent) {
        try {
            if (broadcastIntentMethod.getParameterTypes().length == 11) {
                broadcastIntentMethod.invoke(
                    manager, null, paramIntent, null, null, 0, null, null, null, Boolean.TRUE, Boolean.FALSE, -2);
            } else if (broadcastIntentMethod.getParameterTypes().length == 12) {
                broadcastIntentMethod.invoke(
                    manager, null, paramIntent, null, null, 0, null, null, null, -1, Boolean.TRUE, Boolean.FALSE, -2);
            } else if (broadcastIntentMethod.getParameterTypes().length == 13) {
                broadcastIntentMethod.invoke(
                    manager, null, paramIntent, null, null, 0, null, null, null, -1, null, Boolean.TRUE, Boolean.FALSE, -2);
            }
        }catch (Exception e){
            throw new AssertionError(e);
        }
    }

    //Currently not working
    public Intent registerReceiver(IIntentReceiver receiver, IntentFilter intentFilter) {
        try {
            return (Intent)registerReceiverMethod.invoke(manager, null, null, receiver, intentFilter, null, -2, 0);
        }catch (Exception e){
            throw new AssertionError(e);
        }
    }
}
