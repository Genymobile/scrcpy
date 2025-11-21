package com.genymobile.scrcpy.util;

import com.genymobile.scrcpy.AndroidVersions;

import android.annotation.SuppressLint;
import android.os.Build;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;

import androidx.annotation.RequiresApi;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.Scanner;

import libcore.io.Libcore;

public final class IO {
    private IO() {
        // not instantiable
    }

    // https://github.com/Genymobile/scrcpy/issues/291
    // https://android.googlesource.com/platform/libcore/+/lollipop-mr1-release/luni/src/main/java/libcore/io/Posix.java#253
    static void maybeUpdateBufferPosition(ByteBuffer buffer, int originalPosition, int bytesReadOrWritten) {
        if (bytesReadOrWritten > 0) {
            buffer.position(bytesReadOrWritten + originalPosition);
        }
    }

    @SuppressWarnings("deprecation")
    private static final class Api19 {

        static int write(FileDescriptor fd, ByteBuffer from) throws IOException {
            while (true) {
                try {
                    final int position = from.position();
                    final int bytesWritten = Libcore.os.write(fd, from);
                    maybeUpdateBufferPosition(from, position, bytesWritten);
                    return bytesWritten;
                } catch (libcore.io.ErrnoException e) {
                    if (e.errno != libcore.io.OsConstants.EINTR) {
                        throw e.rethrowAsIOException();
                    }
                }
            }
        }

        static boolean isBrokenPipe(Throwable cause) {
            return cause instanceof libcore.io.ErrnoException
                    && ((libcore.io.ErrnoException) cause).errno == libcore.io.OsConstants.EPIPE;
        }

        private Api19() {
            // not instantiable
        }
    }

    @RequiresApi(AndroidVersions.API_21_ANDROID_5_0)
    private static final class Api21 {

        static int write(FileDescriptor fd, ByteBuffer from) throws IOException {
            while (true) {
                try {
                    final int position = from.position();
                    final int bytesWritten = Os.write(fd, from);
                    maybeUpdateBufferPosition(from, position, bytesWritten);
                    return bytesWritten;
                } catch (ErrnoException e) {
                    if (e.errno != OsConstants.EINTR) {
                        throw rethrowAsIOException(e);
                    }
                }
            }
        }

        @SuppressLint("NewApi") // ErrnoException.rethrowAsIOException() is @hide until API 30.
        static IOException rethrowAsIOException(ErrnoException e) throws IOException {
            return e.rethrowAsIOException();
        }

        static boolean isBrokenPipe(Throwable cause) {
            return cause instanceof ErrnoException
                    && ((ErrnoException) cause).errno == OsConstants.EPIPE;
        }

        private Api21() {
            // not instantiable
        }
    }

    @RequiresApi(AndroidVersions.API_22_ANDROID_5_1)
    private static final class Api22 {

        static int write(FileDescriptor fd, ByteBuffer from) throws IOException {
            while (true) {
                try {
                    return Os.write(fd, from);
                } catch (ErrnoException e) {
                    if (e.errno != OsConstants.EINTR) {
                        throw Api21.rethrowAsIOException(e);
                    }
                }
            }
        }

        private Api22() {
            // not instantiable
        }
    }

    public static void writeFully(FileDescriptor fd, ByteBuffer from) throws IOException {
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_22_ANDROID_5_1) {
            while (from.hasRemaining()) {
                Api22.write(fd, from);
            }
        } else if (Build.VERSION.SDK_INT == AndroidVersions.API_21_ANDROID_5_0) {
            while (from.hasRemaining()) {
                Api21.write(fd, from);
            }
        } else {
            while (from.hasRemaining()) {
                Api19.write(fd, from);
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
        Throwable cause = e.getCause();
        if (Build.VERSION.SDK_INT >= AndroidVersions.API_21_ANDROID_5_0) {
            return Api21.isBrokenPipe(cause);
        } else {
            return Api19.isBrokenPipe(cause);
        }
    }

    public static boolean isBrokenPipe(Exception e) {
        return e instanceof IOException && isBrokenPipe((IOException) e);
    }
}
