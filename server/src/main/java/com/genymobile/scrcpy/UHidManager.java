package com.genymobile.scrcpy;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.HandlerThread;
import android.os.MessageQueue;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import java.io.FileDescriptor;
import java.io.InterruptedIOException;
import java.nio.ByteBuffer;
import java.util.Hashtable;

public class UHidManager {
    private final Hashtable<Integer, FileDescriptor> mHandles = new Hashtable<>();
    private final HandlerThread thread = new HandlerThread("UHidManager");
    private final MessageQueue queue;
    private final DeviceMessageSender sender;
    private final ByteBuffer buffer = ByteBuffer.allocateDirect(5 * 1024);

    @TargetApi(Build.VERSION_CODES.M)
    public UHidManager(DeviceMessageSender sender) {
        thread.start();
        queue = thread.getLooper().getQueue();
        this.sender = sender;
    }

    public void stop() {
        thread.quit();
    }

    public void join() {
        try {
            thread.join();
        } catch (InterruptedException e) {
            Ln.e("Failed to join UHidManager thread: ", e);
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    public void open(int id) {
        FileDescriptor fd = null;
        try {
            fd = Os.open("/dev/uhid", OsConstants.O_RDWR, 0);
        } catch (ErrnoException e) {
            Ln.e("Failed to open uhid", e);
            return;
        }

        mHandles.put(id, fd);
        queue.addOnFileDescriptorEventListener(fd, MessageQueue.OnFileDescriptorEventListener.EVENT_INPUT | MessageQueue.OnFileDescriptorEventListener.EVENT_OUTPUT, (fd2, event) -> {
            if (event == MessageQueue.OnFileDescriptorEventListener.EVENT_INPUT) {
                try {
                    buffer.rewind();
                    Os.read(fd2, buffer);
                    // create a copy of buffer up to its current position
                    byte[] data = new byte[buffer.position()];
                    buffer.rewind();
                    buffer.get(data);
                    Ln.v("UHidManager: read " + data.length + " bytes from " + id);
                    sender.pushUHidData(id, data);
                } catch (ErrnoException | InterruptedIOException e) {
                    return 0;
                }
            }
            return MessageQueue.OnFileDescriptorEventListener.EVENT_INPUT;
        });
    }

    public void write(int id, byte[] data) {
        FileDescriptor fd = mHandles.get(id);
        if (fd == null) {
            return;
        }

        try {
            Os.write(fd, data, 0, data.length);
        } catch (Exception e) {
            Ln.e("Failed to write to uhid: " + e.getMessage());
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    public void close(int id) {
        FileDescriptor fd = mHandles.get(id);
        if (fd == null) {
            return;
        }
        queue.removeOnFileDescriptorEventListener(fd);
        try {
            Os.close(fd);
        } catch (Exception e) {
            Ln.e("Failed to close uhid: " + e.getMessage());
        }
    }
}
