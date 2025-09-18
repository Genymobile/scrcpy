package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.AndroidVersions;
import com.genymobile.scrcpy.BuildConfig;

import android.os.Build;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import androidx.annotation.RequiresApi;

import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Scanner;

public final class IO {
    private IO() {
        // not instantiable
    }

    @RequiresApi(AndroidVersions.API_21_ANDROID_5_0)
    private static class Api21 {

        static int write(FileDescriptor fd, ByteBuffer from) throws IOException {
            while (true) {
                try {
                    return Os.write(fd, from);
                } catch (ErrnoException e) {
                    if (e.errno != OsConstants.EINTR) {
                        throw new IOException(e);
                    }
                }
            }
        }

        private Api21() {
        }
    }

    public static void writeFully(FileDescriptor fd, ByteBuffer from) throws IOException {
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_23_ANDROID_6_0) {
            while (from.hasRemaining()) {
                Api21.write(fd, from);
            }
        } else if (Build.VERSION.SDK_INT >= AndroidVersions.API_21_ANDROID_5_0) {
            // ByteBuffer position is not updated as expected by Os.write() on old Android versions, so
            // handle the position and the remaining bytes manually.
            // See <https://github.com/Genymobile/scrcpy/issues/291>.
            int position = from.position();
            int remaining = from.remaining();
            while (remaining > 0) {
                int w = Api21.write(fd, from);
                if (BuildConfig.DEBUG && w < 0) {
                    // w should not be negative, since an exception is thrown on error
                    throw new AssertionError("Os.write() returned a negative value (" + w + ")");
                }
                remaining -= w;
                position += w;
                from.position(position);
            }
        } else {
            try (FileOutputStream output = new FileOutputStream(fd)) {
                try (FileChannel channel = output.getChannel()) {
                    int position = from.position();
                    int remaining = from.remaining();
                    while (remaining > 0) {
                        try {
                            int w = channel.write(from);
                            remaining -= w;
                            position += w;
                            from.position(position);
                        } catch (IOException e) {
                            if (e.getMessage() == null || !e.getMessage().contains("EINTR")) {
                                throw e;
                            }
                        }
                    }
                }
            }
        }
    }

    public static void writeFully(FileDescriptor fd, byte[] buffer, int offset, int len) throws IOException {
        writeFully(fd, ByteBuffer.wrap(buffer, offset, len));
    }

    public static String toString(InputStream inputStream) {
        StringBuilder builder = new StringBuilder();
        Scanner scanner = new Scanner(inputStream);
        while (scanner.hasNextLine()) {
            builder.append(scanner.nextLine()).append('\n');
        }
        return builder.toString();
    }

    public static boolean isBrokenPipe(IOException e) {
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_21_ANDROID_5_0) {
            Throwable cause = e.getCause();
            return cause instanceof ErrnoException && ((ErrnoException) cause).errno == OsConstants.EPIPE;
        } else {
            String message = e.getMessage();
            return message != null && message.contains("EPIPE");
        }
    }

    public static boolean isBrokenPipe(Exception e) {
        return e instanceof IOException && isBrokenPipe((IOException) e);
    }
}
