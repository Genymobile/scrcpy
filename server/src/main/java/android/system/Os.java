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

package android.system;

import android.annotation.TargetApi;

import java.io.FileDescriptor;
import java.io.InterruptedIOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.net.SocketException;
import java.nio.ByteBuffer;

import libcore.io.Libcore;

/**
 * Access to low-level system functionality. Most of these are system calls. Most users will want
 * to use higher-level APIs where available, but this class provides access to the underlying
 * primitives used to implement the higher-level APIs.
 *
 * <p>The corresponding constants can be found in {@link OsConstants}.
 */
@SuppressWarnings("unused")
@TargetApi(21)
public final class Os {
  private Os() {}

  /**
   * See <a href="http://man7.org/linux/man-pages/man2/write.2.html">write(2)</a>.
   */
  public static int write(FileDescriptor fd, ByteBuffer buffer) throws ErrnoException, InterruptedIOException {
    try {
      return Libcore.os.write(fd, buffer);
    } catch (libcore.io.ErrnoException e) {
      throw new ErrnoException("write", e.errno);
    }
  }
}
