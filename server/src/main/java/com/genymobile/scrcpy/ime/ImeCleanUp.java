package com.genymobile.scrcpy.ime;

import com.genymobile.scrcpy.util.Command;
import com.genymobile.scrcpy.util.Ln;

import android.system.ErrnoException;
import android.system.Os;

import java.io.IOException;

public final class ImeCleanUp {
    private ImeCleanUp() {
        // not instantiable
    }

    public static void main(String... args) {
        try {
            Os.setsid();
        } catch (ErrnoException e) {
            Ln.e("setsid() failed", e);
        }

        String originalIme = args[0];
        boolean wasEnabled = Boolean.parseBoolean(args[1]);

        try {
            while (System.in.read() != -1) {
                // Wait for the parent process to close its pipe or exit.
            }
        } catch (IOException e) {
            // Expected when the parent process dies.
        }

        try {
            String currentIme = Command.execReadLine("settings", "get", "secure", "default_input_method");
            if (currentIme != null && ImeManager.IME_ID.equals(currentIme.trim())) {
                Ln.i("Restoring input method: " + originalIme);
                Command.exec("ime", "set", originalIme);
            }
        } catch (IOException e) {
            Ln.e("Could not restore input method", e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }

        if (!wasEnabled) {
            try {
                Command.exec("ime", "disable", ImeManager.IME_ID);
            } catch (IOException e) {
                Ln.e("Could not restore scrcpy IME enabled state", e);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        System.exit(0);
    }
}
