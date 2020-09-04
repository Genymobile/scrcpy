/*
 * Copyright 2007-2014 the original author or authors.
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

public class Logger implements Appendable {

    private final boolean quiet;

    public Logger(boolean quiet) {
        this.quiet = quiet;
    }
    
    public void log(String message) {
        if (!quiet) {
            System.out.println(message);
        }
    }

    public Appendable append(CharSequence csq) {
        if (!quiet) {
            System.out.append(csq);
        }
        return this;
    }

    public Appendable append(CharSequence csq, int start, int end) {
        if (!quiet) {
            System.out.append(csq, start, end);
        }
        return this;
    }

    public Appendable append(char c) {
        if(!quiet) {
            System.out.append(c);
        }
        return this;
    }
}
