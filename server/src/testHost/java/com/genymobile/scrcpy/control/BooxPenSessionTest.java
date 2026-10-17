package com.genymobile.scrcpy.control;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Exercises the worker's actual control loop and EOF cleanup without Android or device writes. */
public final class BooxPenSessionTest {
    private static void sample(DataOutputStream out, int action) throws IOException {
        out.writeInt(action);
        out.writeLong(-4);
        out.writeFloat(.25f);
        out.writeFloat(.75f);
        out.writeFloat(.5f);
    }

    public static void main(String[] args) throws Exception {
        for (boolean truncated : new boolean[] {false, true}) {
            ByteArrayOutputStream wire = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(wire);
            sample(out, 0);
            sample(out, 2);
            if (truncated) {
                out.writeInt(2); // EOF halfway through the next packet.
            }
            ByteArrayOutputStream events = new ByteArrayOutputStream();
            BooxPen pen = new BooxPen(events, true, 0, 100, 0, 200, 4095, 20, 30, 48, 0);
            boolean failed = false;
            try {
                BooxPenSession.run(new ByteArrayInputStream(wire.toByteArray()), pen, () -> 1L);
            } catch (IOException e) {
                failed = true;
            }
            if (failed != truncated || events.size() != 3 * 6 * 24) {
                throw new AssertionError("EOF must release exactly once, including partial packets");
            }
            ByteBuffer bytes = ByteBuffer.wrap(events.toByteArray()).order(ByteOrder.nativeOrder());
            int start = events.size() - 6 * 24;
            if (bytes.getInt(start + 20) != 0 || bytes.getShort(start + 24 + 18) != 330
                    || bytes.getInt(start + 24 + 20) != 0) {
                throw new AssertionError("Worker EOF did not clear pressure/contact");
            }
        }
        System.out.println("BooxPenSession: EOF and truncated-stream cleanup fixtures passed");
    }
}
