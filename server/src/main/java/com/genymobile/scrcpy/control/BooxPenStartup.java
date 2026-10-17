package com.genymobile.scrcpy.control;

import java.io.Closeable;
import java.io.IOException;
import java.util.function.Consumer;

/** Own startup separately from control-channel reception. Pending pen packets are discarded, never queued. */
final class BooxPenStartup implements Closeable {
    interface Writer extends Closeable {
        void inject(int action, long pointer, float x, float y, float pressure) throws IOException;
    }

    interface Factory {
        Writer create() throws IOException, InterruptedException;
    }

    private final Thread thread;
    private Writer writer;
    private boolean stopped;

    BooxPenStartup(Factory factory, Consumer<Exception> failure) {
        thread = new Thread(() -> {
            try {
                Writer created = factory.create();
                synchronized (this) {
                    if (!stopped) {
                        writer = created;
                        return;
                    }
                }
                created.close(); // Cancellation won the race with the factory: never publish this writer.
            } catch (IOException | InterruptedException | RuntimeException e) {
                synchronized (this) {
                    if (stopped) {
                        return;
                    }
                    stopped = true;
                }
                failure.accept(e);
            }
        }, "boox-startup");
    }

    synchronized void start() {
        if (!stopped) {
            thread.start();
        }
    }

    synchronized boolean isReady() {
        return !stopped && writer != null;
    }

    synchronized boolean inject(int action, long pointer, float x, float y, float pressure) throws IOException {
        if (stopped || writer == null) {
            return false;
        }
        writer.inject(action, pointer, x, y, pressure);
        return true;
    }

    @Override
    public void close() throws IOException {
        Writer toClose;
        synchronized (this) {
            stopped = true;
            thread.interrupt();
            toClose = writer;
            writer = null;
        }
        if (toClose != null) {
            toClose.close();
        }
    }

    void join() throws InterruptedException {
        thread.join();
    }
}
