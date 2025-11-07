/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.view;

import android.graphics.Rect;
import android.content.res.Configuration;

import java.util.List;

/**
 * Interface to listen for changes to display window-containers.
 *
 * This differs from DisplayManager's DisplayListener in a couple ways:
 *  - onDisplayAdded is always called after the display is actually added to the WM hierarchy.
 *    This corresponds to the DisplayContent and not the raw Dislay from DisplayManager.
 *  - onDisplayConfigurationChanged is called for all configuration changes, not just changes
 *    to displayinfo (eg. windowing-mode).
 *
 */
oneway interface IDisplayWindowListener {

    /**
     * Called when a new display is added to the WM hierarchy. The existing display ids are returned
     * when this listener is registered with WM via {@link #registerDisplayWindowListener}.
     */
    void onDisplayAdded(int displayId);

    /**
     * Called when a display's window-container configuration has changed.
     */
    void onDisplayConfigurationChanged(int displayId, in Configuration newConfig);

    /**
     * Called when a display is removed from the hierarchy.
     */
    void onDisplayRemoved(int displayId);
}
