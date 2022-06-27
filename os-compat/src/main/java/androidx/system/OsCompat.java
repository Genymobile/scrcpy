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

import java.io.FileDescriptor;
import java.io.InterruptedIOException;
import java.nio.ByteBuffer;

import static android.os.Build.VERSION.SDK_INT;

/**
 * Access to low-level system functionality. Most of these are system calls. Most users will want
 * to use higher-level APIs where available, but this class provides access to the underlying
 * primitives used to implement the higher-level APIs.
 *
 * <p>The corresponding constants can be found in {@link OsConstantsCompat}.
 */
public final class OsCompat {
    private OsCompat() {
    }

    private static final Os IMPL;

    static {
        if (SDK_INT >= 21) {
            IMPL = new OsApi21();
        } else {
            IMPL = new OsLibcore();
        }
    }

    /**
     * See <a href="http://man7.org/linux/man-pages/man3/strerror.3.html">strerror(2)</a>.
     */
    public static String strerror(int errno) {
        return IMPL.strerror(errno);
    }

    /**
     * See <a href="http://man7.org/linux/man-pages/man2/write.2.html">write(2)</a>.
     */
    public static int write(FileDescriptor fd, ByteBuffer buffer) throws ErrnoException, InterruptedIOException {
        return IMPL.write(fd, buffer);
    }
}
