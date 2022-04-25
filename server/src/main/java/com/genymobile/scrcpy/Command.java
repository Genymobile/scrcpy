package com.genymobile.scrcpy;

import java.io.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Scanner;

public final class Command {
    private Command() {
        // not instantiable
    }

    public static void exec(String... cmd) throws IOException, InterruptedException {
        Process process = Runtime.getRuntime().exec(cmd);
        int exitCode = process.waitFor();
        if (exitCode != 0) {
            throw new IOException("Command " + Arrays.toString(cmd) + " returned with value " + exitCode);
        }
    }

    public static void execShellScript(String script, String... args) throws IOException, InterruptedException {

        ArrayList<String> cmd = new ArrayList<>();
        cmd.add("sh");
        cmd.add("-s");
        cmd.addAll(Arrays.asList(args));

        Process process = Runtime.getRuntime().exec(cmd.toArray(new String[]{}));
        BufferedReader err = new BufferedReader(new InputStreamReader(process.getErrorStream()));
        BufferedReader output = new BufferedReader(new InputStreamReader(process.getInputStream()));
        BufferedWriter input = new BufferedWriter(new OutputStreamWriter(process.getOutputStream()));
        input.write(script);
        input.close();

        StringBuilder fullLines = new StringBuilder();
        String line;
        if (Ln.isEnabled(Ln.Level.DEBUG)) {
            while ((line = output.readLine()) != null) {
                fullLines.append(line);
                fullLines.append("\n");
            }
            Ln.d("Custom script output:\n---\n" + fullLines + "\n----\n");
        }
        fullLines = new StringBuilder();
        if (Ln.isEnabled(Ln.Level.WARN)) {
            while ((line = err.readLine()) != null) {
                fullLines.append(line);
                fullLines.append("\n");
            }
            Ln.w("Custom script err:\n---\n" + fullLines + "\n----\n");
        }

        int exitCode = process.waitFor();
        if (exitCode != 0) {
            throw new IOException("Custom script with args: " + Arrays.toString(args) + " returned with value " + exitCode);
        }
    }

    public static String execReadLine(String... cmd) throws IOException, InterruptedException {
        String result = null;
        Process process = Runtime.getRuntime().exec(cmd);
        Scanner scanner = new Scanner(process.getInputStream());
        if (scanner.hasNextLine()) {
            result = scanner.nextLine();
        }
        int exitCode = process.waitFor();
        if (exitCode != 0) {
            throw new IOException("Command " + Arrays.toString(cmd) + " returned with value " + exitCode);
        }
        return result;
    }
}
