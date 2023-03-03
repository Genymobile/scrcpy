package com.genymobile.scrcpy;

public interface AsyncProcessor {
    void start();
    void stop();
    void join() throws InterruptedException;
}
