package com.genymobile.scrcpy;

import android.media.MediaCodec;
import android.os.Build;

import java.io.IOException;
import java.nio.ByteBuffer;

public final class AudioRawRecorder implements AsyncProcessor {

    private final Streamer streamer;

    private Thread thread;

    private static final int READ_MS = 5; // milliseconds
    private static final int READ_SIZE = AudioCapture.millisToBytes(READ_MS);

    public AudioRawRecorder(Streamer streamer) {
        this.streamer = streamer;
    }

    private void record() throws IOException, AudioCaptureForegroundException {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            Ln.w("Audio disabled: it is not supported before Android 11");
            streamer.writeDisableStream(false);
            return;
        }

        final ByteBuffer buffer = ByteBuffer.allocateDirect(READ_SIZE);
        final MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();

        AudioCapture capture = new AudioCapture();
        try {
            capture.start();

            streamer.writeAudioHeader();
            while (!Thread.currentThread().isInterrupted()) {
                buffer.position(0);
                int r = capture.read(buffer, READ_SIZE, bufferInfo);
                if (r < 0) {
                    throw new IOException("Could not read audio: " + r);
                }
                buffer.limit(r);

                streamer.writePacket(buffer, bufferInfo);
            }
        } catch (Throwable e) {
            // Notify the client that the audio could not be captured
            streamer.writeDisableStream(false);
            throw e;
        } finally {
            capture.stop();
        }
    }

    @Override
    public void start(TerminationListener listener) {
        thread = new Thread(() -> {
            boolean fatalError = false;
            try {
                record();
            } catch (AudioCaptureForegroundException e) {
                // Do not print stack trace, a user-friendly error-message has already been logged
            } catch (IOException e) {
                Ln.e("Audio recording error", e);
                fatalError = true;
            } finally {
                Ln.d("Audio recorder stopped");
                listener.onTerminated(fatalError);
            }
        });
        thread.start();
    }

    @Override
    public void stop() {
        if (thread != null) {
            thread.interrupt();
        }
    }

    @Override
    public void join() throws InterruptedException {
        if (thread != null) {
            thread.join();
        }
    }
}
