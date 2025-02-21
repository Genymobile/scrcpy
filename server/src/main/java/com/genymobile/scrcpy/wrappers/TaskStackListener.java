/**
 * Copyright (c) 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.genymobile.scrcpy.wrappers;

import android.app.ActivityManager;
import android.app.ITaskStackListener;
import android.content.ComponentName;
import android.os.IBinder;
 
public class TaskStackListener extends ITaskStackListener.Stub {
    @Override
    public void onTaskStackChanged() {
        // empty default implementation
    }

    @Override
    public void onActivityPinned(String packageName, int userId, int taskId, int stackId) {
        // empty default implementation
    }

    @Override
    public void onActivityUnpinned() {
        // empty default implementation
    }
    
    @Override
    public void onActivityRestartAttempt(ActivityManager.RunningTaskInfo task, boolean homeTaskVisible,
            boolean clearedTask, boolean wasVisible) {
        // empty default implementation
    }
    
    @Override
    public void onActivityForcedResizable(String packageName, int taskId, int reason) {
        // empty default implementation
    }
    
    @Override
    public void onActivityDismissingDockedStack() {
        // empty default implementation
    }
    
    @Override
    public void onActivityLaunchOnSecondaryDisplayFailed(ActivityManager.RunningTaskInfo taskInfo,
            int requestedDisplayId) {
        // empty default implementation
    }
    
    @Override
    public void onActivityLaunchOnSecondaryDisplayRerouted(ActivityManager.RunningTaskInfo taskInfo,
        int requestedDisplayId) {
        // empty default implementation
    }
    
    @Override
    public void onTaskCreated(int taskId, ComponentName componentName) {
        // empty default implementation
    }

    @Override
    public void onTaskRemoved(int taskId) {
        // empty default implementation
    }

    @Override
    public void onTaskMovedToFront(ActivityManager.RunningTaskInfo taskInfo) {
        // empty default implementation
    }

    @Override
    public void onTaskDescriptionChanged(ActivityManager.RunningTaskInfo taskInfo) {
        // empty default implementation
    }

    @Override
    public void onActivityRequestedOrientationChanged(int taskId, int requestedOrientation) {
        // empty default implementation
    }

    @Override
    public void onTaskRemovalStarted(ActivityManager.RunningTaskInfo taskInfo) {
        // empty default implementation
    }


    @Override
    public void onTaskProfileLocked(int taskId, int userId) {
        // empty default implementation
    }

    @Override
    public void onTaskSnapshotChanged(int taskId, ActivityManager.TaskSnapshot snapshot) {
        // empty default implementation
    }

    @Override
    public void onSizeCompatModeActivityChanged(int displayId, IBinder activityToken) {
        // empty default implementation
    }

    @Override
    public void onBackPressedOnTaskRoot(ActivityManager.RunningTaskInfo taskInfo) {
        // empty default implementation
    }

    @Override
    public void onSingleTaskDisplayDrawn(int displayId) {
        // empty default implementation
    }

    @Override
    public void onSingleTaskDisplayEmpty(int displayId) {
        // empty default implementation
    }

    @Override
    public void onTaskDisplayChanged(int taskId, int newDisplayId) {
        // empty default implementation
    }

    @Override
    public void onRecentTaskListUpdated() {
        // empty default implementation
    }

    @Override
    public void onRecentTaskListFrozenChanged(boolean frozen) {
        // empty default implementation
    }

    @Override
    public void onTaskFocusChanged(int taskId, boolean focused) {
        // empty default implementation
    }

    @Override
    public void onTaskRequestedOrientationChanged(int taskId, int requestedOrientation) {
        // empty default implementation
    }

    @Override
    public void onActivityRotation(int displayId) {
        // empty default implementation
    }
}