package com.genymobile.scrcpy;

public interface AsyncProcessor {
    interface TerminationListener {
        /**
         * Notify processor termination
         *
         * @param fatalError {@code true} if this must cause the termination of the whole scrcpy-server.
         */
        void onTerminated(boolean fatalError);
    }

    void start(TerminationListener listener);

    void stop();

    void join() throws InterruptedException;
}
