package com.genymobile.scrcpy.control;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.function.LongSupplier;

/** The private parent/worker pipe. EOF or malformed/truncated packets always close/release the pen. */
public final class BooxPenSession {
    private BooxPenSession() {
    }

    public static void run(InputStream stream, BooxPen pen, LongSupplier clock) throws IOException {
        try (BooxPen ownedPen = pen) {
            // Read available pipe bytes in bulk; never wait to accumulate a full buffer or stroke.
            DataInputStream in = new DataInputStream(new BufferedInputStream(stream));
            while (true) {
                int first = in.read();
                if (first == -1) {
                    return;
                }
                // Only clean EOF between packets is normal. A partial action header is an error too.
                int action = first << 24 | in.readUnsignedByte() << 16 | in.readUnsignedByte() << 8 | in.readUnsignedByte();
                long pointer = in.readLong();
                float x = in.readFloat();
                float y = in.readFloat();
                float pressure = in.readFloat();
                if (action < 0 || action > 3) {
                    throw new IOException("Invalid BOOX worker action");
                }
                ownedPen.inject(action, pointer, x, y, pressure, clock.getAsLong());
            }
        }
    }
}
