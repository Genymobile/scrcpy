/*
 * Copyright 2007-2008 the original author or authors.
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
import java.math.BigInteger;
import java.net.URI;
import java.security.MessageDigest;

public class PathAssembler {
    public static final String GRADLE_USER_HOME_STRING = "GRADLE_USER_HOME";
    public static final String PROJECT_STRING = "PROJECT";

    private File gradleUserHome;

    public PathAssembler() {
    }

    public PathAssembler(File gradleUserHome) {
        this.gradleUserHome = gradleUserHome;
    }

    /**
     * Determines the local locations for the distribution to use given the supplied configuration.
     */
    public LocalDistribution getDistribution(WrapperConfiguration configuration) {
        String baseName = getDistName(configuration.getDistribution());
        String distName = removeExtension(baseName);
        String rootDirName = rootDirName(distName, configuration);
        File distDir = new File(getBaseDir(configuration.getDistributionBase()), configuration.getDistributionPath() + "/" + rootDirName);
        File distZip = new File(getBaseDir(configuration.getZipBase()), configuration.getZipPath() + "/" + rootDirName + "/" + baseName);
        return new LocalDistribution(distDir, distZip);
    }

    private String rootDirName(String distName, WrapperConfiguration configuration) {
        String urlHash = getHash(Download.safeUri(configuration.getDistribution()).toString());
        return distName + "/" + urlHash;
    }

    /**
     * This method computes a hash of the provided {@code string}.
     * <p>
     * The algorithm in use by this method is as follows:
     * <ol>
     *    <li>Compute the MD5 value of {@code string}.</li>
     *    <li>Truncate leading zeros (i.e., treat the MD5 value as a number).</li>
     *    <li>Convert to base 36 (the characters {@code 0-9a-z}).</li>
     * </ol>
     */
    private String getHash(String string) {
        try {
            MessageDigest messageDigest = MessageDigest.getInstance("MD5");
            byte[] bytes = string.getBytes();
            messageDigest.update(bytes);
            return new BigInteger(1, messageDigest.digest()).toString(36);
        } catch (Exception e) {
            throw new RuntimeException("Could not hash input string.", e);
        }
    }

    private String removeExtension(String name) {
        int p = name.lastIndexOf(".");
        if (p < 0) {
            return name;
        }
        return name.substring(0, p);
    }

    private String getDistName(URI distUrl) {
        String path = distUrl.getPath();
        int p = path.lastIndexOf("/");
        if (p < 0) {
            return path;
        }
        return path.substring(p + 1);
    }

    private File getBaseDir(String base) {
        if (base.equals(GRADLE_USER_HOME_STRING)) {
            return gradleUserHome;
        } else if (base.equals(PROJECT_STRING)) {
            return new File(System.getProperty("user.dir"));
        } else {
            throw new RuntimeException("Base: " + base + " is unknown");
        }
    }

    public static class LocalDistribution {
        private final File distZip;
        private final File distDir;

        public LocalDistribution(File distDir, File distZip) {
            this.distDir = distDir;
            this.distZip = distZip;
        }

        /**
         * Returns the location to install the distribution into.
         */
        public File getDistributionDir() {
            return distDir;
        }

        /**
         * Returns the location to install the distribution ZIP file to.
         */
        public File getZipFile() {
            return distZip;
        }
    }
}
