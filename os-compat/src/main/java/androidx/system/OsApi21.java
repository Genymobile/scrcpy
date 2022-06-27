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

import androidx.annotation.RequiresApi;

import java.io.FileDescriptor;
import java.io.InterruptedIOException;
import java.nio.ByteBuffer;

import static android.os.Build.VERSION.SDK_INT;
import static androidx.system.Os.maybeUpdateBufferPosition;

@RequiresApi(21)
final class OsApi21 implements Os {
    @Override
    public String strerror(int errno) {
        return android.system.Os.strerror(errno);
    }

    @Override
    public int write(FileDescriptor fd, ByteBuffer buffer) throws ErrnoException, InterruptedIOException {
        try {
            final int position = buffer.position();
            final int bytesWritten = android.system.Os.write(fd, buffer);
            if (SDK_INT < 22) {
                maybeUpdateBufferPosition(buffer, position, bytesWritten);
            }
            return bytesWritten;
        } catch (android.system.ErrnoException e) {
            throw new ErrnoException("write", e.errno);
        }
    }
}
