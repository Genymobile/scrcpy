package com.genymobile.scrcpy.android;

/** Monotonically identifies the active connection so delayed callbacks become harmless. */
final class SessionGuard {
    private long generation;

    synchronized long start() {
        return ++generation;
    }

    synchronized long invalidate() {
        return ++generation;
    }

    synchronized boolean isCurrent(long expectedGeneration) {
        return generation == expectedGeneration;
    }

    synchronized long current() {
        return generation;
    }
}
