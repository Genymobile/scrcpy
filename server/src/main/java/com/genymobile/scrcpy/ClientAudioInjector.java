package com.genymobile.scrcpy;

import com.genymobile.scrcpy.audio.AudioDecoder;
import com.genymobile.scrcpy.audio.LatestAudioBuffer;
import com.genymobile.scrcpy.device.DesktopConnection;
import com.genymobile.scrcpy.util.Ln;

import android.net.LocalSocket;

import java.io.BufferedInputStream;
import java.io.IOException;

/**
 * Receive Opus audio from the client and inject it into the device microphone.
 * <p>
 * A failure never terminates the server: the client audio socket is closed instead (it is the only signal the client can observe),
 * while video, audio and control continue.
 */
public final class ClientAudioInjector implements AsyncProcessor {

    // Four stereo PCM frames (about 80 ms). If Android consumes more slowly, retain the newest speech instead of accumulating latency.
    private static final int PCM_BUFFER_SIZE = 4 * 4096;

    private final DesktopConnection connection;

    private Thread thread;

    public ClientAudioInjector(DesktopConnection connection) {
        this.connection = connection;
    }

    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            try {
                run();
            } catch (Exception e) {
                Ln.e("Client audio injection error", e);
            } finally {
                closeSocket();
                Ln.d("Client audio injection stopped");
                listener.onTerminated(false);
            }
        }, "client-audio");
        thread.start();
    }

    private void run() throws Exception {
        LocalSocket socket = connection.getClientAudioSocket();
        if (socket == null) {
            // Already stopped
            return;
        }

        BufferedInputStream input = new BufferedInputStream(socket.getInputStream());
        LatestAudioBuffer pcm = new LatestAudioBuffer(PCM_BUFFER_SIZE);
        AudioDecoder decoder = new AudioDecoder();
        Thread decoderThread = decoder.start(input, pcm);
        try {
            // The injector thread terminates when the decoder closes the PCM buffer (end of stream or error), or on injection error
            Thread injectorThread = AudioInjector.injectAudio(pcm, this::closeSocket);
            injectorThread.join();
        } finally {
            // Unblock the decoder if it is still reading from the socket
            closeSocket();
            decoderThread.join();
        }
    }

    private void closeSocket() {
        try {
            connection.closeClientAudio();
        } catch (IOException e) {
            Ln.w("Could not close client audio socket", e);
        }
    }

    @Override
    public void stop() {
        if (thread != null) {
            closeSocket();
        }
    }

    @Override
    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }
}
