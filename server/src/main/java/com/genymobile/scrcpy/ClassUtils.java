package com.genymobile.scrcpy;

import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;

public class ClassUtils {
    public static String whereAmI(Class<?> c) { // https://stackoverflow.com/a/6219855/12857692
        final String classPaths[] = System.getProperty("java.class.path").split(System.getProperty("path.separator"));
        final String miNombre = c.getName();
        for (String p: classPaths) {
            final File fichero = new File(p);
            try {
                final URL url = fichero.toURI().toURL();
                final URL[] urls = new URL[]{url};
                final ClassLoader cl = new URLClassLoader(urls);
                cl.loadClass(miNombre);
            } catch (Exception e) {
                continue;
            }
            return fichero.getAbsolutePath();
        }
        return null;
    }
}
