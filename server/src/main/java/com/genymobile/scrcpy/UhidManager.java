package com.genymobile.scrcpy;

import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.ArrayMap;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

public final class UhidManager {

    // Linux: include/uapi/linux/uhid.h
    private static final int UHID_CREATE2 = 11;
    private static final int UHID_INPUT2 = 12;

    // Linux: include/uapi/linux/input.h
    private static final short BUS_VIRTUAL = 0x06;

    private final ArrayMap<Integer, FileDescriptor> fds = new ArrayMap<>();

    public void open(int id, byte[] reportDesc) throws IOException {
        try {
            FileDescriptor fd = Os.open("/dev/uhid", OsConstants.O_RDWR, 0);
            try {
                FileDescriptor old = fds.put(id, fd);
                if (old != null) {
                    Ln.w("Duplicate UHID id: " + id);
                    close(old);
                }

                byte[] req = buildUhidCreate2Req(reportDesc);
                Os.write(fd, req, 0, req.length);
            } catch (Exception e) {
                close(fd);
                throw e;
            }
        } catch (ErrnoException e) {
            throw new IOException(e);
        }
    }

    public void writeInput(int id, byte[] data) throws IOException {
        FileDescriptor fd = fds.get(id);
        if (fd == null) {
            Ln.w("Unknown UHID id: " + id);
            return;
        }

        try {
            byte[] req = buildUhidInput2Req(data);
            Os.write(fd, req, 0, req.length);
        } catch (ErrnoException e) {
            throw new IOException(e);
        }
    }

    private static byte[] buildUhidCreate2Req(byte[] reportDesc) {
        /*
         * struct uhid_event {
         *     uint32_t type;
         *     union {
         *         // ...
         *         struct uhid_create2_req {
         *             uint8_t name[128];
         *             uint8_t phys[64];
         *             uint8_t uniq[64];
         *             uint16_t rd_size;
         *             uint16_t bus;
         *             uint32_t vendor;
         *             uint32_t product;
         *             uint32_t version;
         *             uint32_t country;
         *             uint8_t rd_data[HID_MAX_DESCRIPTOR_SIZE];
         *         };
         *     };
         * } __attribute__((__packed__));
         */

        byte[] empty = new byte[256];
        ByteBuffer buf = ByteBuffer.allocate(280 + reportDesc.length).order(ByteOrder.nativeOrder());
        buf.putInt(UHID_CREATE2);
        buf.put("scrcpy".getBytes(StandardCharsets.US_ASCII));
        buf.put(empty, 0, 256 - "scrcpy".length());
        buf.putShort((short) reportDesc.length);
        buf.putShort(BUS_VIRTUAL);
        buf.putInt(0); // vendor id
        buf.putInt(0); // product id
        buf.putInt(0); // version
        buf.putInt(0); // country;
        buf.put(reportDesc);
        return buf.array();
    }

    private static byte[] buildUhidInput2Req(byte[] data) {
        /*
         * struct uhid_event {
         *     uint32_t type;
         *     union {
         *         // ...
         *         struct uhid_input2_req {
         *             uint16_t size;
         *             uint8_t data[UHID_DATA_MAX];
         *         };
         *     };
         * } __attribute__((__packed__));
         */

        ByteBuffer buf = ByteBuffer.allocate(6 + data.length).order(ByteOrder.nativeOrder());
        buf.putInt(UHID_INPUT2);
        buf.putShort((short) data.length);
        buf.put(data);
        return buf.array();
    }

    public void close(int id) {
        FileDescriptor fd = fds.get(id);
        assert fd != null;
        close(fd);
    }

    public void closeAll() {
        for (FileDescriptor fd : fds.values()) {
            close(fd);
        }
    }

    private static void close(FileDescriptor fd) {
        try {
            Os.close(fd);
        } catch (ErrnoException e) {
            Ln.e("Failed to close uhid: " + e.getMessage());
        }
    }
}
