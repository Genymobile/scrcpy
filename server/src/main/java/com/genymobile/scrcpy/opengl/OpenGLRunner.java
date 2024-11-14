package com.genymobile.scrcpy.opengl;

import com.genymobile.scrcpy.device.Size;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLExt;
import android.opengl.EGLSurface;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;

import java.util.concurrent.Semaphore;

public final class OpenGLRunner {

    private static HandlerThread handlerThread;
    private static Handler handler;
    private static boolean quit;

    private EGLDisplay eglDisplay;
    private EGLContext eglContext;
    private EGLSurface eglSurface;

    private final OpenGLFilter filter;
    private final float[] overrideTransformMatrix;

    private SurfaceTexture surfaceTexture;
    private Surface inputSurface;
    private int textureId;

    private boolean stopped;

    public OpenGLRunner(OpenGLFilter filter, float[] overrideTransformMatrix) {
        this.filter = filter;
        this.overrideTransformMatrix = overrideTransformMatrix;
    }

    public OpenGLRunner(OpenGLFilter filter) {
        this(filter, null);
    }

    public static synchronized void initOnce() {
        if (handlerThread == null) {
            if (quit) {
                throw new IllegalStateException("Could not init OpenGLRunner after it is quit");
            }
            handlerThread = new HandlerThread("OpenGLRunner");
            handlerThread.start();
            handler = new Handler(handlerThread.getLooper());
        }
    }

    public static void quit() {
        HandlerThread thread;
        synchronized (OpenGLRunner.class) {
            thread = handlerThread;
            quit = true;
        }
        if (thread != null) {
            thread.quitSafely();
        }
    }

    public static void join() throws InterruptedException {
        HandlerThread thread;
        synchronized (OpenGLRunner.class) {
            thread = handlerThread;
        }
        if (thread != null) {
            thread.join();
        }
    }

    public Surface start(Size inputSize, Size outputSize, Surface outputSurface) throws OpenGLException {
        initOnce();

        // Simulate CompletableFuture, but working for all Android versions
        final Semaphore sem = new Semaphore(0);
        Throwable[] throwableRef = new Throwable[1];

        // The whole OpenGL execution must be performed on a Handler, so that SurfaceTexture.setOnFrameAvailableListener() works correctly.
        // See <https://github.com/Genymobile/scrcpy/issues/5444>
        handler.post(() -> {
            try {
                run(inputSize, outputSize, outputSurface);
            } catch (Throwable throwable) {
                throwableRef[0] = throwable;
            } finally {
                sem.release();
            }
        });

        try {
            sem.acquire();
        } catch (InterruptedException e) {
            // Behave as if this method call was synchronous
            Thread.currentThread().interrupt();
        }

        Throwable throwable = throwableRef[0];
        if (throwable != null) {
            if (throwable instanceof OpenGLException) {
                throw (OpenGLException) throwable;
            }
            throw new OpenGLException("Asynchronous OpenGL runner init failed", throwable);
        }

        // Synchronization is ok: inputSurface is written before sem.release() and read after sem.acquire()
        return inputSurface;
    }

    private void run(Size inputSize, Size outputSize, Surface outputSurface) throws OpenGLException {
        eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (eglDisplay == EGL14.EGL_NO_DISPLAY) {
            throw new OpenGLException("Unable to get EGL14 display");
        }

        int[] version = new int[2];
        if (!EGL14.eglInitialize(eglDisplay, version, 0, version, 1)) {
            throw new OpenGLException("Unable to initialize EGL14");
        }

        // @formatter:off
        int[] attribList = {
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGL14.EGL_NONE
        };

        EGLConfig[] configs = new EGLConfig[1];
        int[] numConfigs = new int[1];
        EGL14.eglChooseConfig(eglDisplay, attribList, 0, configs, 0, configs.length, numConfigs, 0);
        if (numConfigs[0] <= 0) {
            EGL14.eglTerminate(eglDisplay);
            throw new OpenGLException("Unable to find ES2 EGL config");
        }
        EGLConfig eglConfig = configs[0];

        // @formatter:off
        int[] contextAttribList = {
                EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL14.EGL_NONE
        };
        eglContext = EGL14.eglCreateContext(eglDisplay, eglConfig, EGL14.EGL_NO_CONTEXT, contextAttribList, 0);
        if (eglContext == null) {
            EGL14.eglTerminate(eglDisplay);
            throw new OpenGLException("Failed to create EGL context");
        }

        int[] surfaceAttribList = {
                EGL14.EGL_NONE
        };
        eglSurface = EGL14.eglCreateWindowSurface(eglDisplay, eglConfig, outputSurface, surfaceAttribList, 0);
        if (eglSurface == null) {
            EGL14.eglDestroyContext(eglDisplay, eglContext);
            EGL14.eglTerminate(eglDisplay);
            throw new OpenGLException("Failed to create EGL window surface");
        }

        if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            EGL14.eglDestroySurface(eglDisplay, eglSurface);
            EGL14.eglDestroyContext(eglDisplay, eglContext);
            EGL14.eglTerminate(eglDisplay);
            throw new OpenGLException("Failed to make EGL context current");
        }

        int[] textures = new int[1];
        GLES20.glGenTextures(1, textures, 0);
        GLUtils.checkGlError();
        textureId = textures[0];

        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLUtils.checkGlError();
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLUtils.checkGlError();
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLUtils.checkGlError();
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        GLUtils.checkGlError();

        surfaceTexture = new SurfaceTexture(textureId);
        surfaceTexture.setDefaultBufferSize(inputSize.getWidth(), inputSize.getHeight());
        inputSurface = new Surface(surfaceTexture);

        filter.init();

        surfaceTexture.setOnFrameAvailableListener(surfaceTexture -> {
            if (stopped) {
                // Make sure to never render after resources have been released
                return;
            }

            render(outputSize);
        }, handler);
    }

    private void render(Size outputSize) {
        GLES20.glViewport(0, 0, outputSize.getWidth(), outputSize.getHeight());
        GLUtils.checkGlError();

        surfaceTexture.updateTexImage();

        float[] matrix;
        if (overrideTransformMatrix != null) {
            matrix = overrideTransformMatrix;
        } else {
            matrix = new float[16];
            surfaceTexture.getTransformMatrix(matrix);
        }

        filter.draw(textureId, matrix);

        EGLExt.eglPresentationTimeANDROID(eglDisplay, eglSurface, surfaceTexture.getTimestamp());
        EGL14.eglSwapBuffers(eglDisplay, eglSurface);
    }

    public void stopAndRelease() {
        final Semaphore sem = new Semaphore(0);

        handler.post(() -> {
            stopped = true;
            surfaceTexture.setOnFrameAvailableListener(null, handler);

            filter.release();

            int[] textures = {textureId};
            GLES20.glDeleteTextures(1, textures, 0);
            GLUtils.checkGlError();

            EGL14.eglDestroySurface(eglDisplay, eglSurface);
            EGL14.eglDestroyContext(eglDisplay, eglContext);
            EGL14.eglTerminate(eglDisplay);
            eglDisplay = EGL14.EGL_NO_DISPLAY;
            eglContext = EGL14.EGL_NO_CONTEXT;
            eglSurface = EGL14.EGL_NO_SURFACE;
            surfaceTexture.release();
            inputSurface.release();

            sem.release();
        });

        try {
            sem.acquire();
        } catch (InterruptedException e) {
            // Behave as if this method call was synchronous
            Thread.currentThread().interrupt();
        }
    }
}
