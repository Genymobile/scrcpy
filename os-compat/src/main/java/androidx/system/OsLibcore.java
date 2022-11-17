/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package androidx.system;

import libcore.io.Libcore;
import libcore.io.OsConstants;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.ByteBuffer;

import static androidx.system.Os.maybeUpdateBufferPosition;

final class OsLibcore implements Os {
    @Override
    public String strerror(int errno) {
        return Libcore.os.strerror(errno);
    }

    @Override
    public int write(FileDescriptor fd, ByteBuffer buffer) throws ErrnoException, InterruptedIOException {
        try {
            return ioFailureRetry("write", fd, () -> {
                final int position = buffer.position();
                final int bytesWritten = Libcore.os.write(fd, buffer);
                maybeUpdateBufferPosition(buffer, position, bytesWritten);
                return bytesWritten;
            });
        } catch (libcore.io.ErrnoException e) {
            throw new ErrnoException("write", e.errno);
        }
    }

    // https://android.googlesource.com/platform/libcore/+/lollipop-release/luni/src/main/native/libcore_io_Posix.cpp#128
    // https://android.googlesource.com/platform/libcore/+/kitkat-release/luni/src/main/java/libcore/io/IoBridge.java#186
    private static int ioFailureRetry(String functionName, FileDescriptor fd, SysCall syscall)
            throws libcore.io.ErrnoException, InterruptedIOException {
        if (!fd.valid()) {
            throw UndeclaredExceptions.raise(new IOException("File descriptor closed"));
        }
        int rc = -1;
        do {
            int syscallErrno = 0;
            try {
                rc = syscall.call();
            } catch (libcore.io.ErrnoException e) {
                syscallErrno = e.errno;
            }
            if (rc == -1 && !fd.valid()) {
                throw new InterruptedIOException(functionName + " interrupted");
            }
            if (rc == -1 && syscallErrno != OsConstants.EINTR) {
                throw new libcore.io.ErrnoException(functionName, syscallErrno);
            }
        } while (rc == -1);
        return rc;
    }

    @FunctionalInterface
    private interface SysCall {

        Integer call() throws libcore.io.ErrnoException;
    }

    // https://dzone.com/articles/throwing-undeclared-checked
    private static final class UndeclaredExceptions extends RuntimeException {
        private static Throwable sThrowable = null;

        public static synchronized RuntimeException raise(Throwable throwable) {
            if (throwable instanceof ReflectiveOperationException || throwable instanceof RuntimeException) {
                throw new IllegalArgumentException("Unsupported exception: " + throwable.getClass());
            }

            sThrowable = throwable;
            try {
                return UndeclaredExceptions.class.newInstance();
            } catch (ReflectiveOperationException e) {
                return new RuntimeException(e);
            } finally {
                sThrowable = null;
            }
        }

        private UndeclaredExceptions() throws Throwable {
            throw sThrowable;
        }
    }
}
