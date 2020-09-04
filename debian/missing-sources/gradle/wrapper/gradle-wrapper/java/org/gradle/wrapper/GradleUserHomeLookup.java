/*
 * Copyright 2014 the original author or authors.
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

import java.io.File;

public class GradleUserHomeLookup {
    public static final String DEFAULT_GRADLE_USER_HOME = System.getProperty("user.home") + "/.gradle";
    public static final String GRADLE_USER_HOME_PROPERTY_KEY = "gradle.user.home";
    public static final String GRADLE_USER_HOME_ENV_KEY = "GRADLE_USER_HOME";

    public static File gradleUserHome() {
        String gradleUserHome;
        if ((gradleUserHome = System.getProperty(GRADLE_USER_HOME_PROPERTY_KEY)) != null) {
            return new File(gradleUserHome);
        }
        if ((gradleUserHome = System.getenv(GRADLE_USER_HOME_ENV_KEY)) != null) {
            return new File(gradleUserHome);
        }
        return new File(DEFAULT_GRADLE_USER_HOME);
    }
}
