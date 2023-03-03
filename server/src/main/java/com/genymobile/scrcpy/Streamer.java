package com.genymobile.scrcpy;

import android.media.MediaCodec;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;

public final class Streamer {

    private static final long PACKET_FLAG_CONFIG = 1L << 63;
    private static final long PACKET_FLAG_KEY_FRAME = 1L << 62;

    private final FileDescriptor fd;
    private final Codec codec;
    private final boolean sendCodecId;
    private final boolean sendFrameMeta;

    private final ByteBuffer headerBuffer = ByteBuffer.allocate(12);

    public Streamer(FileDescriptor fd, Codec codec, boolean sendCodecId, boolean sendFrameMeta) {
        this.fd = fd;
        this.codec = codec;
        this.sendCodecId = sendCodecId;
        this.sendFrameMeta = sendFrameMeta;
    }

    public Codec getCodec() {
        return codec;
    }

    public void writeHeader() throws IOException {
        if (sendCodecId) {
            ByteBuffer buffer = ByteBuffer.allocate(4);
            buffer.putInt(codec.getId());
            buffer.flip();
            IO.writeFully(fd, buffer);
        }
    }

    public void writePacket(ByteBuffer buffer, long pts, boolean config, boolean keyFrame) throws IOException {
        if (sendFrameMeta) {
            writeFrameMeta(fd, buffer.remaining(), pts, config, keyFrame);
        }

        IO.writeFully(fd, buffer);
    }

    public void writePacket(ByteBuffer codecBuffer, MediaCodec.BufferInfo bufferInfo) throws IOException {
        long pts = bufferInfo.presentationTimeUs;
        boolean config = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
        boolean keyFrame = (bufferInfo.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0;
        writePacket(codecBuffer, pts, config, keyFrame);
    }

    private void writeFrameMeta(FileDescriptor fd, int packetSize, long pts, boolean config, boolean keyFrame) throws IOException {
        headerBuffer.clear();

        long ptsAndFlags;
        if (config) {
            ptsAndFlags = PACKET_FLAG_CONFIG; // non-media data packet
        } else {
            ptsAndFlags = pts;
            if (keyFrame) {
                ptsAndFlags |= PACKET_FLAG_KEY_FRAME;
            }
        }

        headerBuffer.putLong(ptsAndFlags);
        headerBuffer.putInt(packetSize);
        headerBuffer.flip();
        IO.writeFully(fd, headerBuffer);
    }
}
