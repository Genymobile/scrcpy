package com.genymobile.scrcpy;

import android.os.Build;
import android.os.HandlerThread;
import android.os.MessageQueue;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.ArrayMap;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

public final class UhidManager {

    // Linux: include/uapi/linux/uhid.h
    private static final int UHID_OUTPUT = 6;
    private static final int UHID_CREATE2 = 11;
    private static final int UHID_INPUT2 = 12;

    // Linux: include/uapi/linux/input.h
    private static final short BUS_VIRTUAL = 0x06;

    private static final int SIZE_OF_UHID_EVENT = 4380; // sizeof(struct uhid_event)

    private final ArrayMap<Integer, FileDescriptor> fds = new ArrayMap<>();
    private final ByteBuffer buffer = ByteBuffer.allocate(SIZE_OF_UHID_EVENT).order(ByteOrder.nativeOrder());

    private final DeviceMessageSender sender;
    private final HandlerThread thread = new HandlerThread("UHidManager");
    private final MessageQueue queue;

    public UhidManager(DeviceMessageSender sender) {
        this.sender = sender;
        thread.start();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            queue = thread.getLooper().getQueue();
        } else {
            queue = null;
        }
    }

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

                registerUhidListener(id, fd);
            } catch (Exception e) {
                close(fd);
                throw e;
            }
        } catch (ErrnoException e) {
            throw new IOException(e);
        }
    }

    private void registerUhidListener(int id, FileDescriptor fd) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            queue.addOnFileDescriptorEventListener(fd, MessageQueue.OnFileDescriptorEventListener.EVENT_INPUT, (fd2, events) -> {
                try {
                    buffer.clear();
                    int r = Os.read(fd2, buffer);
                    buffer.flip();
                    if (r > 0) {
                        int type = buffer.getInt();
                        if (type == UHID_OUTPUT) {
                            byte[] data = extractHidOutputData(buffer);
                            if (data != null) {
                                DeviceMessage msg = DeviceMessage.createUhidOutput(id, data);
                                sender.send(msg);
                            }
                        }
                    }
                } catch (ErrnoException | InterruptedIOException e) {
                    Ln.e("Failed to read UHID output", e);
                    return 0;
                }
                return events;
            });
        }
    }

    private static byte[] extractHidOutputData(ByteBuffer buffer) {
        /*
         * #define UHID_DATA_MAX 4096
         * struct uhid_event {
         *     uint32_t type;
         *     union {
         *         // ...
         *         struct uhid_output_req {
         *             __u8 data[UHID_DATA_MAX];
         *             __u16 size;
         *             __u8 rtype;
         *         };
         *     };
         * } __attribute__((__packed__));
         */

        if (buffer.remaining() < 4099) {
            Ln.w("Incomplete HID output");
            return null;
        }
        int size = buffer.getShort(buffer.position() + 4096) & 0xFFFF;
        if (size > 4096) {
            Ln.w("Incorrect HID output size: " + size);
            return null;
        }
        byte[] data = new byte[size];
        buffer.get(data);
        return data;
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
