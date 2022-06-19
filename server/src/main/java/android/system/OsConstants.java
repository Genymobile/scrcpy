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

@SuppressWarnings("unused")
@TargetApi(21)
public final class OsConstants {

    private OsConstants() { }

    public static final int EINTR = libcore.io.OsConstants.EINTR;

    /**
     * Returns the string name of an errno value.
     * For example, "EACCES". See {@link Os#strerror} for human-readable errno descriptions.
     */
    public static String errnoName(int errno) {
        return libcore.io.OsConstants.errnoName(errno);
    }
}
