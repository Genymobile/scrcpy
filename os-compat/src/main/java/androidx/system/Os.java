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

interface Os {
    String strerror(int errno);

    int write(FileDescriptor fd, ByteBuffer buffer) throws ErrnoException, InterruptedIOException;

    // https://android.googlesource.com/platform/libcore/+/lollipop-mr1-release/luni/src/main/java/libcore/io/Posix.java#253
    static void maybeUpdateBufferPosition(ByteBuffer buffer, int originalPosition, int bytesReadOrWritten) {
        if (bytesReadOrWritten > 0) {
            buffer.position(bytesReadOrWritten + originalPosition);
        }
    }
}
