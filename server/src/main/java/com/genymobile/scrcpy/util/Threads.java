package com.genymobile.scrcpy.util;

import android.os.Handler;

import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;

public final class Threads {
    private Threads() {
        // not instantiable
    }

    public static <T> T executeSynchronouslyOn(Handler handler, Callable<T> callable) throws Throwable {
        // Simulate CompletableFuture, but working for all Android versions
        final Semaphore sem = new Semaphore(0);
        @SuppressWarnings("unchecked")
        T[] resultRef = (T[]) new Object[1];
        Throwable[] throwableRef = new Throwable[1];

        handler.post(() -> {
            try {
                resultRef[0] = callable.call();
            } catch (Throwable throwable) {
                throwableRef[0] = throwable;
            } finally {
                sem.release();
            }
        });

        try {
            sem.acquire();
        } catch (InterruptedException e) {
            // Behave as if this method call was synchronous
            Thread.currentThread().interrupt();
        }

        if (throwableRef[0] != null) {
            throw throwableRef[0];
        }

        return resultRef[0];
    }
}
