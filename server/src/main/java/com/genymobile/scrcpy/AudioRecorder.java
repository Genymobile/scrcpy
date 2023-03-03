package com.genymobile.scrcpy;

/**
 * A component able to record audio asynchronously
 *
 * The implementation is responsible to send packets.
 */
public interface AudioRecorder {
    void start();
    void stop();
    void join() throws InterruptedException;
}
