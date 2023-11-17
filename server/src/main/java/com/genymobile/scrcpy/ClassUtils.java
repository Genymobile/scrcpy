package com.genymobile.scrcpy;

import dalvik.system.DexClassLoader;
import java.io.File;

public class ClassUtils {
    public static String whereAmI(Class<?> c) { // https://stackoverflow.com/a/10833005/12857692
        final String classPaths[] = System.getProperty("java.class.path").split(System.getProperty("path.separator"));
        final String myName = c.getName();
        final File cwd = new File(System.getProperty("user.dir"));
        final ClassLoader cl = ClassLoader.getSystemClassLoader().getParent();
        for (String p: classPaths) {
            File file = new File(p);
            if (!file.isAbsolute()) {
                file = new File(cwd, file.getPath());
            }
            try {
                final DexClassLoader dLoader = new DexClassLoader(file.getAbsolutePath(), file.getParentFile().getAbsolutePath(), null, cl);
                dLoader.loadClass(myName);
                return file.getAbsolutePath();
            } catch (Exception e) {
                Ln.d(e.getMessage());
            }
        }
        return null;
    }
}
