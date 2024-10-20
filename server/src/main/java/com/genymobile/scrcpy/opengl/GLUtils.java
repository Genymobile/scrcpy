package com.genymobile.scrcpy.opengl;

import com.genymobile.scrcpy.BuildConfig;
import com.genymobile.scrcpy.util.Ln;

import android.opengl.GLES20;
import android.opengl.GLU;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

public final class GLUtils {

    private static final boolean DEBUG = BuildConfig.DEBUG;

    private GLUtils() {
        // not instantiable
    }

    public static int createProgram(String vertexSource, String fragmentSource) {
        int vertexShader = createShader(GLES20.GL_VERTEX_SHADER, vertexSource);
        if (vertexShader == 0) {
            return 0;
        }

        int fragmentShader = createShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
        if (fragmentShader == 0) {
            GLES20.glDeleteShader(vertexShader);
            return 0;
        }

        int program = GLES20.glCreateProgram();
        if (program == 0) {
            GLES20.glDeleteShader(fragmentShader);
            GLES20.glDeleteShader(vertexShader);
            return 0;
        }

        GLES20.glAttachShader(program, vertexShader);
        checkGlError();
        GLES20.glAttachShader(program, fragmentShader);
        checkGlError();
        GLES20.glLinkProgram(program);
        checkGlError();

        int[] linkStatus = new int[1];
        GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
        if (linkStatus[0] == 0) {
            Ln.e("Could not link program: " + GLES20.glGetProgramInfoLog(program));
            GLES20.glDeleteProgram(program);
            GLES20.glDeleteShader(fragmentShader);
            GLES20.glDeleteShader(vertexShader);
            return 0;
        }

        return program;
    }

    public static int createShader(int type, String source) {
        int shader = GLES20.glCreateShader(type);
        if (shader == 0) {
            Ln.e(getGlErrorMessage("Could not create shader"));
            return 0;
        }

        GLES20.glShaderSource(shader, source);
        GLES20.glCompileShader(shader);

        int[] compileStatus = new int[1];
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compileStatus, 0);
        if (compileStatus[0] == 0) {
            Ln.e("Could not compile " + getShaderTypeString(type) + ": " + GLES20.glGetShaderInfoLog(shader));
            GLES20.glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    private static String getShaderTypeString(int type) {
        switch (type) {
            case GLES20.GL_VERTEX_SHADER:
                return "vertex shader";
            case GLES20.GL_FRAGMENT_SHADER:
                return "fragment shader";
            default:
                return "shader";
        }
    }

    /**
     * Throws a runtime exception if {@link GLES20#glGetError()} returns an error (useful for debugging).
     */
    public static void checkGlError() {
        if (DEBUG) {
            int error = GLES20.glGetError();
            if (error != GLES20.GL_NO_ERROR) {
                throw new RuntimeException(toErrorString(error));
            }
        }
    }

    public static String getGlErrorMessage(String userError) {
        int glError = GLES20.glGetError();
        if (glError == GLES20.GL_NO_ERROR) {
            return userError;
        }

        return userError + " (" + toErrorString(glError) + ")";
    }

    private static String toErrorString(int glError) {
        String errorString = GLU.gluErrorString(glError);
        return "glError 0x" + Integer.toHexString(glError) + " " + errorString;
    }

    public static FloatBuffer createFloatBuffer(float[] values) {
        FloatBuffer fb = ByteBuffer.allocateDirect(values.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
        fb.put(values);
        fb.position(0);
        return fb;
    }
}
