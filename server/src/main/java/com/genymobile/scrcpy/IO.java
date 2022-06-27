package com.genymobile.scrcpy;

import androidx.system.ErrnoException;
import androidx.system.OsCompat;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;

public final class IO {
    private IO() {
        // not instantiable
    }

    public static void writeFully(FileDescriptor fd, ByteBuffer from) throws IOException {
        try {
            while (from.hasRemaining()) {
                OsCompat.write(fd, from);
            }
        } catch (ErrnoException e) {
            throw new IOException(e);
        }
    }

    public static void writeFully(FileDescriptor fd, byte[] buffer, int offset, int len) throws IOException {
        writeFully(fd, ByteBuffer.wrap(buffer, offset, len));
    }
}
