/*
 * Copyright 2017 the original author or authors.
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
package org.gradle.wrapper;

import java.net.URI;

public interface DownloadProgressListener {
    /**
     * Reports the current progress of the download
     *
     * @param address       distribution url
     * @param contentLength the content length of the distribution, or -1 if the content length is not known.
     * @param downloaded    the total amount of currently downloaded bytes
     */
    void downloadStatusChanged(URI address, long contentLength, long downloaded);
}
