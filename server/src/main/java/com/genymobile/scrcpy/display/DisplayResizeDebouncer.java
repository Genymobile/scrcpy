package com.genymobile.scrcpy.display;

import com.genymobile.scrcpy.model.Size;
import com.genymobile.scrcpy.util.Ln;

public final class DisplayResizeDebouncer {

    private static final long DEBOUNCE_DELAY_MS = 300;

    public interface Callback {
        void trigger(Size size);
    }

    private final Callback callback;
    private Size request;
    private long deadline;

    public DisplayResizeDebouncer(Callback callback) {
        this.callback = callback;

        Thread thread = new Thread(this::debounce);
        thread.setName("debouncer");
        thread.setDaemon(true);
        thread.start();
    }

    private void debounce() {
        try {
            while (true) {
                Size newSize;
                synchronized (this) {
                    while (true) {
                        long now = nowMs();
                        if (request != null && now >= deadline) {
                            break;
                        }
                        if (request == null) {
                            wait();
                        } else {
                            assert now < deadline;
                            wait(deadline - now);
                        }
                    }
                    assert request != null : "An active deadline implies request != null";
                    newSize = request;
                    request = null;
                }
                callback.trigger(newSize);
            }
        } catch (InterruptedException e) {
            Ln.e("Unexpected interruption", e);
        }
    }

    public synchronized void requestResize(Size size) {
        assert size != null;
        if (request == null) {
            deadline = nowMs() + DEBOUNCE_DELAY_MS;
        }
        request = size;
        notify();
    }

    public synchronized void cancelResize() {
        request = null;
        deadline = 0;
        notify();
    }

    private long nowMs() {
        return System.nanoTime() / 1000000;
    }
}
