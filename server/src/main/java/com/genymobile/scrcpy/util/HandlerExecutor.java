package com.genymobile.scrcpy.util;

import android.os.Handler;

import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

// Inspired from hidden android.os.HandlerExecutor

public class HandlerExecutor implements Executor {
    private final Handler handler;

    public HandlerExecutor(Handler handler) {
        this.handler = handler;
    }

    @Override
    public void execute(Runnable command) {
        if (!handler.post(command)) {
            throw new RejectedExecutionException(handler + " is shutting down");
        }
    }
}
