package com.genymobile.scrcpy;

import android.media.MediaCodec;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;

public final class VideoStreamer {

    private static final long PACKET_FLAG_CONFIG = 1L << 63;
    private static final long PACKET_FLAG_KEY_FRAME = 1L << 62;

    private final FileDescriptor fd;
    private final VideoCodec codec;
    private final boolean sendCodecId;
    private final boolean sendFrameMeta;

    private final ByteBuffer headerBuffer = ByteBuffer.allocate(12);

    public VideoStreamer(FileDescriptor fd, VideoCodec codec, boolean sendCodecId, boolean sendFrameMeta) {
        this.fd = fd;
        this.codec = codec;
        this.sendCodecId = sendCodecId;
        this.sendFrameMeta = sendFrameMeta;
    }

    public void writeHeader() throws IOException {
        if (sendCodecId) {
            ByteBuffer buffer = ByteBuffer.allocate(4);
            buffer.putInt(codec.getId());
            buffer.flip();
            IO.writeFully(fd, buffer);
        }
    }

    public void writePacket(ByteBuffer codecBuffer, MediaCodec.BufferInfo bufferInfo) throws IOException {
        if (sendFrameMeta) {
            writeFrameMeta(fd, bufferInfo, codecBuffer.remaining());
        }

        IO.writeFully(fd, codecBuffer);
    }

    private void writeFrameMeta(FileDescriptor fd, MediaCodec.BufferInfo bufferInfo, int packetSize) throws IOException {
        headerBuffer.clear();

        long ptsAndFlags;
        if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
            ptsAndFlags = PACKET_FLAG_CONFIG; // non-media data packet
        } else {
            ptsAndFlags = bufferInfo.presentationTimeUs;
            if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
                ptsAndFlags |= PACKET_FLAG_KEY_FRAME;
            }
        }

        headerBuffer.putLong(ptsAndFlags);
        headerBuffer.putInt(packetSize);
        headerBuffer.flip();
        IO.writeFully(fd, headerBuffer);
    }
}
