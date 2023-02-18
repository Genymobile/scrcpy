package com.genymobile.scrcpy;

import android.media.MediaCodec;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;

public final class Streamer {

    private static final long PACKET_FLAG_CONFIG = 1L << 63;
    private static final long PACKET_FLAG_KEY_FRAME = 1L << 62;

    private static final long AOPUSHDR = 0x5244485355504F41L; // "AOPUSHDR" in ASCII (little-endian)

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

    public void writeDisableStream() throws IOException {
        // Writing 0 (32-bit) as codec-id means that the device disables the stream (because it could not capture)
        byte[] zeros = new byte[4];
        IO.writeFully(fd, zeros, 0, zeros.length);
    }

    public void writePacket(ByteBuffer buffer, long pts, boolean config, boolean keyFrame) throws IOException {
        if (config && codec == AudioCodec.OPUS) {
            fixOpusConfigPacket(buffer);
        }

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

    private static void fixOpusConfigPacket(ByteBuffer buffer) throws IOException {
        // Here is an example of the config packet received for an OPUS stream:
        //
        // 00000000  41 4f 50 55 53 48 44 52  13 00 00 00 00 00 00 00  |AOPUSHDR........|
        // -------------- BELOW IS THE PART WE MUST PUT AS EXTRADATA  -------------------
        // 00000010  4f 70 75 73 48 65 61 64  01 01 38 01 80 bb 00 00  |OpusHead..8.....|
        // 00000020  00 00 00                                          |...             |
        // ------------------------------------------------------------------------------
        // 00000020           41 4f 50 55 53  44 4c 59 08 00 00 00 00  |   AOPUSDLY.....|
        // 00000030  00 00 00 a0 2e 63 00 00  00 00 00 41 4f 50 55 53  |.....c.....AOPUS|
        // 00000040  50 52 4c 08 00 00 00 00  00 00 00 00 b4 c4 04 00  |PRL.............|
        // 00000050  00 00 00                                          |...|
        //
        // Each "section" is prefixed by a 64-bit ID and a 64-bit length.
        //
        // <https://developer.android.com/reference/android/media/MediaCodec#CSD>

        if (buffer.remaining() < 16) {
            throw new IOException("Not enough data in OPUS config packet");
        }

        long id = buffer.getLong();
        if (id != AOPUSHDR) {
            throw new IOException("OPUS header not found");
        }

        long sizeLong = buffer.getLong();
        if (sizeLong < 0 || sizeLong >= 0x7FFFFFFF) {
            throw new IOException("Invalid block size in OPUS header: " + sizeLong);
        }

        int size = (int) sizeLong;
        if (buffer.remaining() < size) {
            throw new IOException("Not enough data in OPUS header (invalid size: " + size + ")");
        }

        // Set the buffer to point to the OPUS header slice
        buffer.limit(buffer.position() + size);
    }
}
