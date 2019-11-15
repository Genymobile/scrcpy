/*
 * Copyright (C) 2006 The Android Open Source Project
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
package android.content;
import android.content.Intent;
import android.os.Bundle;
/**
 * System private API for dispatching intent broadcasts.  This is given to the
 * activity manager as part of registering for an intent broadcasts, and is
 * called when it receives intents.
 *
 * {@hide}
 */
oneway interface IIntentReceiver {
    void performReceive(in Intent intent, int resultCode, String data,
            in Bundle extras, boolean ordered, boolean sticky, int sendingUser);
}