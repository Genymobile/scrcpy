/*
 * Copyright 2012 the original author or authors.
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

public class WrapperConfiguration {
    private URI distribution;
    private String distributionBase = PathAssembler.GRADLE_USER_HOME_STRING;
    private String distributionPath = Install.DEFAULT_DISTRIBUTION_PATH;
    private String distributionSha256Sum;
    private String zipBase = PathAssembler.GRADLE_USER_HOME_STRING;
    private String zipPath = Install.DEFAULT_DISTRIBUTION_PATH;

    public URI getDistribution() {
        return distribution;
    }

    public void setDistribution(URI distribution) {
        this.distribution = distribution;
    }

    public String getDistributionBase() {
        return distributionBase;
    }

    public void setDistributionBase(String distributionBase) {
        this.distributionBase = distributionBase;
    }

    public String getDistributionPath() {
        return distributionPath;
    }

    public void setDistributionPath(String distributionPath) {
        this.distributionPath = distributionPath;
    }

    public String getDistributionSha256Sum() {
        return distributionSha256Sum;
    }

    public void setDistributionSha256Sum(String distributionSha256Sum) {
        this.distributionSha256Sum = distributionSha256Sum;
    }

    public String getZipBase() {
        return zipBase;
    }

    public void setZipBase(String zipBase) {
        this.zipBase = zipBase;
    }

    public String getZipPath() {
        return zipPath;
    }

    public void setZipPath(String zipPath) {
        this.zipPath = zipPath;
    }
}
