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

import java.io.IOException;
import java.net.SocketException;

/**
 * A checked exception thrown when {@link OsCompat} methods fail. This exception contains the native
 * errno value, for comparison against the constants in {@link OsConstantsCompat}, should sophisticated
 * callers need to adjust their behavior based on the exact failure.
 */
public final class ErrnoException extends Exception {
    private final String functionName;
    private final int errno;

    /**
     * Constructs an instance with the given function name and errno value.
     */
    public ErrnoException(String functionName, int errno) {
        this.functionName = functionName;
        this.errno = errno;
    }

    /**
     * Constructs an instance with the given function name, errno value, and cause.
     */
    public ErrnoException(String functionName, int errno, Throwable cause) {
        super(cause);
        this.functionName = functionName;
        this.errno = errno;
    }

    /**
     * The errno value, for comparison with the {@code E} constants in {@link OsConstantsCompat}.
     */
    public int getErrno() {
        return errno;
    }

    /**
     * Converts the stashed function name and errno value to a human-readable string.
     * We do this here rather than in the constructor so that callers only pay for
     * this if they need it.
     */
    @Override public String getMessage() {
        String errnoName = OsConstantsCompat.errnoName(errno);
        if (errnoName == null) {
            errnoName = "errno " + errno;
        }
        String description = OsCompat.strerror(errno);
        return functionName + " failed: " + errnoName + " (" + description + ")";
    }

    /**
     * Throws an {@link IOException} with a message based on {@link #getMessage()} and with this
     * instance as the cause.
     *
     * <p>This method always terminates by throwing the exception. Callers can write
     * {@code throw e.rethrowAsIOException()} to make that clear to the compiler.
     */
    public IOException rethrowAsIOException() throws IOException {
        throw new IOException(getMessage(), this);
    }

    /**
     * Throws a {@link SocketException} with a message based on {@link #getMessage()} and with this
     * instance as the cause.
     *
     * <p>This method always terminates by throwing the exception. Callers can write
     * {@code throw e.rethrowAsIOException()} to make that clear to the compiler.
     */
    public SocketException rethrowAsSocketException() throws SocketException {
        final SocketException newException = new SocketException(getMessage());
        newException.initCause(this);
        throw newException;
    }
}
