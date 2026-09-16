package com.genymobile.scrcpy.audio;

import android.media.MediaCodec;
import android.media.MediaFormat;
import com.genymobile.scrcpy.util.Ln;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class AudioDecoder {
    private MediaCodec decoder;
    private boolean running = false;

    public void start(BufferedInputStream bis, OutputStream pcmOutput) throws IOException {
        // Initialize Opus decoder
        decoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_AUDIO_OPUS);
        MediaFormat format = MediaFormat.createAudioFormat(
            MediaFormat.MIMETYPE_AUDIO_OPUS,
            AudioConfig.SAMPLE_RATE,
            AudioConfig.CHANNELS
        );

        // OpusHead structure for stereo 48kHz Opus
        // Format: "OpusHead" + version(1) + channel_count(2) + pre_skip(312) + sample_rate(48000) + output_gain(0) + channel_mapping(0)
        byte[] opusHead = new byte[] {
            0x4F, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64,  // "OpusHead"
            0x01,                                              // Version
            0x02,                                              // Channel count (2 = stereo)
            0x38, 0x01,                                        // Pre-skip (312 samples, little-endian)
            (byte)0x80, (byte)0xBB, 0x00, 0x00,               // Input sample rate (48000 Hz, little-endian)
            0x00, 0x00,                                        // Output gain (0 dB)
            0x00                                               // Channel mapping family (0 = mono/stereo)
        };
        format.setByteBuffer("csd-0", ByteBuffer.wrap(opusHead));

        // CSD-1: Pre-skip in nanoseconds (312 samples * 1,000,000,000 / 48,000 = 6,500,000 ns)
        long preSkipNs = 6500000L;
        ByteBuffer csd1 = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
        csd1.putLong(preSkipNs);
        csd1.flip();
        format.setByteBuffer("csd-1", csd1);

        // CSD-2: Seek pre-roll in nanoseconds (80ms for Opus = 3840 samples at 48kHz)
        long seekPreRollNs = 80000000L;
        ByteBuffer csd2 = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
        csd2.putLong(seekPreRollNs);
        csd2.flip();
        format.setByteBuffer("csd-2", csd2);

        decoder.configure(format, null, null, 0);
        decoder.start();

        running = true;

        // Decoding loop
        new Thread(() -> {
            try {
                decode(bis, pcmOutput);
            } catch (Exception e) {
                Ln.e("Opus decoder error", e);
            } finally {
                stop();
            }
        }, "opus-decoder").start();
    }

    private void decode(BufferedInputStream bis, OutputStream pcmOutput) throws IOException {
        byte[] sizeBuffer = new byte[4];
        MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
        long presentationTime = 0;

        while (running) {
            // Feed input packets (non-blocking)
            int inputIndex = decoder.dequeueInputBuffer(0);
            if (inputIndex >= 0) {
                int bytesRead = bis.read(sizeBuffer);
                if (bytesRead == 4) {
                    int packetSize = ((sizeBuffer[0] & 0xFF) << 24) |
                                   ((sizeBuffer[1] & 0xFF) << 16) |
                                   ((sizeBuffer[2] & 0xFF) << 8) |
                                   (sizeBuffer[3] & 0xFF);

                    if (packetSize > 0 && packetSize <= 100000) {
                        byte[] opusData = new byte[packetSize];
                        int actualRead = bis.read(opusData);
                        if (actualRead == packetSize) {
                            ByteBuffer inputBuffer = decoder.getInputBuffer(inputIndex);
                            inputBuffer.clear();
                            inputBuffer.put(opusData);
                            decoder.queueInputBuffer(inputIndex, 0, packetSize, presentationTime, 0);
                            // Increment presentation time (20ms per frame at 48kHz = 960 samples)
                            presentationTime += 20000; // microseconds
                        }
                    }
                } else if (bytesRead == -1) {
                    // End of stream
                    decoder.queueInputBuffer(inputIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                    break;
                }
            }

            // Drain available output
            int outputIndex = decoder.dequeueOutputBuffer(bufferInfo, 0);
            if (outputIndex >= 0) {
                ByteBuffer outputBuffer = decoder.getOutputBuffer(outputIndex);
                if (bufferInfo.size > 0) {
                    byte[] pcmData = new byte[bufferInfo.size];
                    outputBuffer.position(bufferInfo.offset);
                    outputBuffer.limit(bufferInfo.offset + bufferInfo.size);
                    outputBuffer.get(pcmData);
                    pcmOutput.write(pcmData);
                    pcmOutput.flush();
                }
                decoder.releaseOutputBuffer(outputIndex, false);
            } else if (outputIndex == MediaCodec.INFO_TRY_AGAIN_LATER) {
                // No output available yet, continue feeding input
            }
        }
    }

    public void stop() {
        running = false;
        if (decoder != null) {
            try {
                decoder.stop();
                decoder.release();
            } catch (Exception e) {}
            decoder = null;
        }
    }
}
