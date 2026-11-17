package com.genymobile.scrcpy.opengl;

import com.genymobile.scrcpy.Options;
import com.genymobile.scrcpy.model.Size;
import com.genymobile.scrcpy.overlay.AdbCommandMonitor;
import com.genymobile.scrcpy.overlay.AdbOverlayEvent;
import com.genymobile.scrcpy.util.AffineMatrix;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.opengl.GLES20;

import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * OpenGL ES 2.0 filter that renders the display frame (with optional {@link AffineMatrix}
 * transform) and composites a Bumblebee-style ADB Command HUD overlay card at the bottom of the
 * video frame when an ADB command is active.
 */
public class AdbOverlayOpenGLFilter implements OpenGLFilter {

    private final AffineOpenGLFilter baseFilter;
    private final Size outputSize;
    private final Options.AdbOverlayMode mode;
    private final Options.AdbOverlayPosition position;
    private final AdbCommandMonitor monitor;

    private int overlayProgram;
    private FloatBuffer quadVertexBuffer;
    private FloatBuffer overlayTexCoordsBuffer;

    private int vertexPosLoc;
    private int texCoordsInLoc;
    private int overlayTexLoc;
    private int alphaLoc;

    private int overlayTextureId;
    private long cachedEventVersion = -1L;
    private Bitmap hudBitmap;

    private static volatile boolean fontsInitialized;
    private static volatile Typeface cachedMonoTypeface;
    private static volatile Typeface cachedSansTypeface;
    private static final List<Object> PINNED_FONT_FAMILIES = new ArrayList<>();

    private static void ensureFontsInitialized() {
        if (!fontsInitialized) {
            fontsInitialized = true;
            try {
                java.lang.reflect.Method m = Typeface.class.getDeclaredMethod("loadPreinstalledSystemFontMap");
                m.setAccessible(true);
                m.invoke(null);
            } catch (Throwable ignored) {
                // Ignore if unavailable or restricted
            }
            try {
                java.lang.reflect.Field mapField = Typeface.class.getDeclaredField("sSystemFontMap");
                mapField.setAccessible(true);
                Object mapObj = mapField.get(null);
                if (mapObj instanceof java.util.Map) {
                    java.util.Map<?, ?> fontMap = (java.util.Map<?, ?>) mapObj;
                    if (!fontMap.isEmpty()) {
                        Object sans = fontMap.get("sans-serif");
                        if (!(sans instanceof Typeface)) {
                            sans = fontMap.values().iterator().next();
                        }
                        if (sans instanceof Typeface && hasNativeInstance((Typeface) sans)) {
                            cachedSansTypeface = (Typeface) sans;
                            setDefaultTypeface(cachedSansTypeface);
                        }
                        Object mono = fontMap.get("monospace");
                        if (mono instanceof Typeface && hasNativeInstance((Typeface) mono)) {
                            cachedMonoTypeface = (Typeface) mono;
                        }
                    }
                }
            } catch (Throwable ignored) {
                // Ignore if sSystemFontMap is unavailable.
            }
            if (cachedSansTypeface == null) {
                String[] sansPaths = {
                        "/system/fonts/RobotoStatic-Regular.ttf",
                        "/system/fonts/Roboto-Regular.ttf",
                        "/system/fonts/DroidSans.ttf",
                };
                for (String path : sansPaths) {
                    Typeface tf = loadTypefaceFromFile(path);
                    if (tf != null) {
                        cachedSansTypeface = tf;
                        setDefaultTypeface(tf);
                        break;
                    }
                }
            }
            if (cachedMonoTypeface == null) {
                Typeface tf = loadTypefaceFromFile("/system/fonts/DroidSansMono.ttf");
                if (tf != null) {
                    cachedMonoTypeface = tf;
                    if (cachedSansTypeface == null) {
                        cachedSansTypeface = tf;
                        setDefaultTypeface(tf);
                    }
                } else {
                    cachedMonoTypeface = cachedSansTypeface;
                }
            }
        }
    }

    private static boolean hasNativeInstance(Typeface tf) {
        if (tf == null) {
            return false;
        }
        try {
            java.lang.reflect.Field niField = Typeface.class.getDeclaredField("native_instance");
            niField.setAccessible(true);
            return niField.getLong(tf) != 0L;
        } catch (Throwable ignored) {
            return true;
        }
    }

    private static void setDefaultTypeface(Typeface tf) {
        if (tf == null) {
            return;
        }
        try {
            java.lang.reflect.Method setDefault =
                    Typeface.class.getDeclaredMethod("setDefault", Typeface.class);
            setDefault.setAccessible(true);
            setDefault.invoke(null, tf);
        } catch (Throwable ignored) {
            // Ignore reflective fallback failure.
        }
        try {
            java.lang.reflect.Field niField = Typeface.class.getDeclaredField("native_instance");
            niField.setAccessible(true);
            long ni = niField.getLong(tf);
            if (ni != 0L) {
                java.lang.reflect.Method nativeSetDefault =
                        Typeface.class.getDeclaredMethod("nativeSetDefault", long.class);
                nativeSetDefault.setAccessible(true);
                nativeSetDefault.invoke(null, ni);
            }
        } catch (Throwable ignored) {
            // Ignore reflective fallback failure.
        }
    }

    private static Typeface loadTypefaceFromFile(String path) {
        java.io.File file = new java.io.File(path);
        if (!file.isFile()) {
            return null;
        }
        try {
            if (android.os.Build.VERSION.SDK_INT >= 29) {
                android.graphics.fonts.Font font = new android.graphics.fonts.Font.Builder(file).build();
                android.graphics.fonts.FontFamily family =
                        new android.graphics.fonts.FontFamily.Builder(font).build();
                PINNED_FONT_FAMILIES.add(family);
                long familyPtr = 0L;
                try {
                    java.lang.reflect.Method getPtr = family.getClass().getDeclaredMethod("getNativePtr");
                    getPtr.setAccessible(true);
                    Object ptrObj = getPtr.invoke(family);
                    if (ptrObj instanceof Long) {
                        familyPtr = (Long) ptrObj;
                    }
                } catch (Throwable ignored) {
                    // Ignore reflective fallback failure.
                }
                if (familyPtr == 0L) {
                    try {
                        java.lang.reflect.Field ptrField = family.getClass().getDeclaredField("mNativePtr");
                        ptrField.setAccessible(true);
                        familyPtr = ptrField.getLong(family);
                    } catch (Throwable ignored) {
                        // Ignore reflective fallback failure.
                    }
                }
                if (familyPtr != 0L) {
                    long typefacePtr = 0L;
                    try {
                        java.lang.reflect.Method createFromArray =
                                Typeface.class.getDeclaredMethod(
                                        "nativeCreateFromArray", long[].class, long.class, int.class, int.class);
                        createFromArray.setAccessible(true);
                        Object res = createFromArray.invoke(null, new long[] {familyPtr}, 0L, 400, 0);
                        if (res instanceof Long) {
                            typefacePtr = (Long) res;
                        }
                    } catch (Throwable ignored) {
                        // Ignore reflective fallback failure.
                    }
                    if (typefacePtr == 0L) {
                        try {
                            java.lang.reflect.Method createFromArrayLegacy =
                                    Typeface.class.getDeclaredMethod(
                                            "nativeCreateFromArray", long[].class, int.class, int.class);
                            createFromArrayLegacy.setAccessible(true);
                            Object res = createFromArrayLegacy.invoke(null, new long[] {familyPtr}, 400, 0);
                            if (res instanceof Long) {
                                typefacePtr = (Long) res;
                            }
                        } catch (Throwable ignored) {
                            // Ignore reflective fallback failure.
                        }
                    }
                    if (typefacePtr != 0L) {
                        java.lang.reflect.Constructor<Typeface> ctor =
                                Typeface.class.getDeclaredConstructor(long.class);
                        ctor.setAccessible(true);
                        return ctor.newInstance(typefacePtr);
                    }
                }
                return new Typeface.CustomFallbackBuilder(family).build();
            }
        } catch (Throwable ignored) {
            // Ignore reflective fallback failure.
        }
        try {
            return Typeface.createFromFile(file);
        } catch (Throwable ignored) {
            return null;
        }
    }

    private static void applySafeTypeface(Paint paint, boolean monospace) {
        ensureFontsInitialized();
        try {
            Typeface tf = monospace
                    ? (cachedMonoTypeface != null ? cachedMonoTypeface : cachedSansTypeface)
                    : (cachedSansTypeface != null ? cachedSansTypeface : cachedMonoTypeface);
            if (tf != null) {
                paint.setTypeface(tf);
            }
        } catch (Throwable ignored) {
            // Leave default Paint typeface if system fonts are uninitialized in app_process
        }
    }

    public AdbOverlayOpenGLFilter(
            AffineMatrix transform,
            Size outputSize,
            Options.AdbOverlayMode mode,
            AdbCommandMonitor monitor) {
        this(transform, outputSize, mode, Options.AdbOverlayPosition.BOTTOM, monitor);
    }

    public AdbOverlayOpenGLFilter(
            AffineMatrix transform,
            Size outputSize,
            Options.AdbOverlayMode mode,
            Options.AdbOverlayPosition position,
            AdbCommandMonitor monitor) {
        AffineMatrix effectiveTransform = transform != null ? transform : AffineMatrix.IDENTITY;
        this.baseFilter = new AffineOpenGLFilter(effectiveTransform);
        this.outputSize = outputSize;
        this.mode = mode;
        this.position = position != null ? position : Options.AdbOverlayPosition.BOTTOM;
        this.monitor = monitor;
    }

    @Override
    public void init() throws OpenGLException {
        ensureFontsInitialized();
        baseFilter.init();

        String vertexShaderCode = "#version 100\n"
                + "attribute vec4 vertex_pos;\n"
                + "attribute vec2 tex_coords_in;\n"
                + "varying vec2 tex_coords;\n"
                + "void main() {\n"
                + "    gl_Position = vertex_pos;\n"
                + "    tex_coords = tex_coords_in;\n"
                + "}";

        String fragmentShaderCode = "#version 100\n"
                + "precision mediump float;\n"
                + "uniform sampler2D overlay_tex;\n"
                + "uniform float u_alpha;\n"
                + "varying vec2 tex_coords;\n"
                + "void main() {\n"
                + "    vec4 c = texture2D(overlay_tex, tex_coords);\n"
                + "    gl_FragColor = c * u_alpha;\n"
                + "}";

        overlayProgram = GLUtils.createProgram(vertexShaderCode, fragmentShaderCode);
        if (overlayProgram == 0) {
            baseFilter.release();
            throw new OpenGLException("Cannot create ADB overlay OpenGL program");
        }

        float[] vertices = {
                -1f, -1f,
                1f, -1f,
                -1f, 1f,
                1f, 1f,
        };

        float[] texCoords = {
                0f, 1f,
                1f, 1f,
                0f, 0f,
                1f, 0f,
        };

        quadVertexBuffer = GLUtils.createFloatBuffer(vertices);
        overlayTexCoordsBuffer = GLUtils.createFloatBuffer(texCoords);

        vertexPosLoc = GLES20.glGetAttribLocation(overlayProgram, "vertex_pos");
        texCoordsInLoc = GLES20.glGetAttribLocation(overlayProgram, "tex_coords_in");
        overlayTexLoc = GLES20.glGetUniformLocation(overlayProgram, "overlay_tex");
        alphaLoc = GLES20.glGetUniformLocation(overlayProgram, "u_alpha");

        int[] textures = new int[1];
        GLES20.glGenTextures(1, textures, 0);
        GLUtils.checkGlError();
        overlayTextureId = textures[0];

        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, overlayTextureId);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        GLUtils.checkGlError();
    }

    @Override
    public void draw(int textureId, float[] texMatrix) {
        baseFilter.draw(textureId, texMatrix);

        if (monitor == null || mode == Options.AdbOverlayMode.OFF) {
            return;
        }

        long nowMs = System.currentTimeMillis();
        List<AdbOverlayEvent> activeEvents = monitor.getActiveEvents(nowMs);
        if (activeEvents.isEmpty()) {
            return;
        }

        float alpha = 0f;
        long combinedVersion = 1L;
        for (AdbOverlayEvent ev : activeEvents) {
            float evAlpha = monitor.computeOverlayAlpha(ev, nowMs);
            if (evAlpha > alpha) {
                alpha = evAlpha;
            }
            combinedVersion = 31L * combinedVersion + ev.getVersion();
        }
        if (alpha <= 0.01f) {
            return;
        }

        if (combinedVersion != cachedEventVersion || hudBitmap == null) {
            hudBitmap = renderHudBitmap(outputSize, activeEvents, mode == Options.AdbOverlayMode.ALL, position, hudBitmap);
            cachedEventVersion = combinedVersion;

            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, overlayTextureId);
            android.opengl.GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, hudBitmap, 0);
            GLUtils.checkGlError();
        }

        GLES20.glEnable(GLES20.GL_BLEND);
        GLES20.glBlendFunc(GLES20.GL_ONE, GLES20.GL_ONE_MINUS_SRC_ALPHA);

        GLES20.glUseProgram(overlayProgram);
        GLUtils.checkGlError();

        GLES20.glEnableVertexAttribArray(vertexPosLoc);
        GLES20.glEnableVertexAttribArray(texCoordsInLoc);

        GLES20.glVertexAttribPointer(vertexPosLoc, 2, GLES20.GL_FLOAT, false, 0, quadVertexBuffer);
        GLES20.glVertexAttribPointer(texCoordsInLoc, 2, GLES20.GL_FLOAT, false, 0, overlayTexCoordsBuffer);

        GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, overlayTextureId);
        GLES20.glUniform1i(overlayTexLoc, 1);
        GLES20.glUniform1f(alphaLoc, alpha);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        GLUtils.checkGlError();

        GLES20.glDisable(GLES20.GL_BLEND);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
    }

    @Override
    public void release() {
        if (overlayTextureId != 0) {
            int[] textures = {overlayTextureId};
            GLES20.glDeleteTextures(1, textures, 0);
            overlayTextureId = 0;
        }
        if (overlayProgram != 0) {
            GLES20.glDeleteProgram(overlayProgram);
            overlayProgram = 0;
        }
        if (hudBitmap != null) {
            hudBitmap.recycle();
            hudBitmap = null;
        }
        baseFilter.release();
    }

    private static float computeCardY(
            int height,
            float cardHeight,
            float marginV,
            Options.AdbOverlayPosition position) {
        if (position == Options.AdbOverlayPosition.TOP) {
            return marginV;
        }
        if (position == Options.AdbOverlayPosition.MIDDLE) {
            return Math.max(marginV, Math.round((height - cardHeight) / 2f));
        }
        return Math.max(marginV, height - cardHeight - marginV);
    }

    public static Bitmap renderHudBitmap(
            Size frameSize,
            AdbOverlayEvent event,
            boolean showAll,
            Bitmap reusableBitmap) {
        return renderHudBitmap(frameSize, event, showAll, Options.AdbOverlayPosition.BOTTOM, reusableBitmap);
    }

    public static Bitmap renderHudBitmap(
            Size frameSize,
            AdbOverlayEvent event,
            boolean showAll,
            Options.AdbOverlayPosition position,
            Bitmap reusableBitmap) {
        List<AdbOverlayEvent> list =
                event != null ? Collections.singletonList(event) : Collections.<AdbOverlayEvent>emptyList();
        return renderHudBitmap(frameSize, list, showAll, position, reusableBitmap);
    }

    public static Bitmap renderHudBitmap(
            Size frameSize,
            List<AdbOverlayEvent> events,
            boolean showAll,
            Bitmap reusableBitmap) {
        return renderHudBitmap(frameSize, events, showAll, Options.AdbOverlayPosition.BOTTOM, reusableBitmap);
    }

    @SuppressWarnings("checkstyle:MethodLength")
    public static Bitmap renderHudBitmap(
            Size frameSize,
            List<AdbOverlayEvent> events,
            boolean showAll,
            Options.AdbOverlayPosition position,
            Bitmap reusableBitmap) {
        ensureFontsInitialized();
        int width = Math.max(320, frameSize.getWidth());
        int height = Math.max(240, frameSize.getHeight());

        Bitmap bitmap = reusableBitmap;
        if (bitmap == null || bitmap.getWidth() != width || bitmap.getHeight() != height || bitmap.isRecycled()) {
            if (bitmap != null && !bitmap.isRecycled()) {
                bitmap.recycle();
            }
            bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        } else {
            bitmap.eraseColor(Color.TRANSPARENT);
        }

        if (events == null || events.isEmpty()) {
            return bitmap;
        }

        if (events.size() > 1) {
            return renderMultiHudBitmap(bitmap, width, height, events, showAll, position);
        }

        AdbOverlayEvent event = events.get(0);

        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);

        float scale = Math.max(0.65f, Math.min(2.0f, width / 1280f));
        float marginX = Math.round(24f * scale);
        float marginBottom = Math.round(20f * scale);
        float cardWidth = width - 2f * marginX;
        float padX = Math.round(18f * scale);
        float lineHeight = Math.round(22f * scale);

        Paint cmdPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(cmdPaint, true);
        cmdPaint.setFakeBoldText(true);
        cmdPaint.setTextSize(15f * scale);
        cmdPaint.setColor(Color.rgb(137, 180, 250));

        float textMaxWidth = Math.max(120f, cardWidth - 2f * padX - Math.round(10f * scale));
        String cmdStr = "$ " + event.getCommand();
        List<String> cmdLines = wrapText(cmdPaint, cmdStr, textMaxWidth, 2);

        Paint outPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(outPaint, true);
        outPaint.setFakeBoldText(true);
        outPaint.setTextSize(14f * scale);

        List<String> outLines = new ArrayList<>();
        if (showAll) {
            String rawOut = event.getOutput();
            String prefix;
            if (event.getStatus() == AdbOverlayEvent.Status.ERROR) {
                outPaint.setColor(Color.rgb(243, 139, 168));
                prefix = "Result (exit " + event.getExitCode() + "): ";
                if (rawOut == null || rawOut.isEmpty()) {
                    rawOut = "(no output)";
                }
            } else if (event.getStatus() == AdbOverlayEvent.Status.RUNNING) {
                outPaint.setColor(Color.rgb(205, 214, 244));
                prefix = "Result: ";
                if (rawOut == null || rawOut.isEmpty()) {
                    rawOut = "Awaiting command output...";
                }
            } else {
                outPaint.setColor(Color.rgb(166, 227, 161));
                prefix = "Result (exit " + event.getExitCode() + "): ";
                if (rawOut == null || rawOut.isEmpty()) {
                    rawOut = "(no output)";
                }
            }
            outLines = wrapText(outPaint, prefix + rawOut, textMaxWidth, 3);
        }

        int totalTextLines = cmdLines.size() + outLines.size();
        float headerHeight = Math.round(46f * scale);
        float bottomPad = Math.round(16f * scale);
        float interSectionGap = showAll ? Math.round(8f * scale) : 0f;
        float cardHeight = headerHeight + totalTextLines * lineHeight + interSectionGap + bottomPad;
        float cardY = computeCardY(height, cardHeight, marginBottom, position);

        Paint cardBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        cardBgPaint.setStyle(Paint.Style.FILL);
        cardBgPaint.setColor(Color.argb(230, 15, 17, 26));
        float cornerRadius = 14f * scale;
        RectF cardRect = new RectF(marginX, cardY, marginX + cardWidth, cardY + cardHeight);
        canvas.drawRoundRect(cardRect, cornerRadius, cornerRadius, cardBgPaint);

        int borderColor;
        int pillBgColor;
        int pillFgColor;
        String statusBadgeText;

        if (event.getStatus() == AdbOverlayEvent.Status.RUNNING) {
            borderColor = Color.rgb(249, 226, 175);
            pillBgColor = Color.argb(64, 249, 226, 175);
            pillFgColor = Color.rgb(249, 226, 175);
            statusBadgeText = "RUNNING...";
        } else if (event.getStatus() == AdbOverlayEvent.Status.ERROR) {
            borderColor = Color.rgb(243, 139, 168);
            pillBgColor = Color.argb(64, 243, 139, 168);
            pillFgColor = Color.rgb(243, 139, 168);
            statusBadgeText = event.getDurationMs() > 0
                    ? "FAILED (exit " + event.getExitCode() + ", " + event.getDurationMs() + "ms)"
                    : "FAILED (exit " + event.getExitCode() + ")";
        } else {
            borderColor = Color.rgb(166, 227, 161);
            pillBgColor = Color.argb(64, 166, 227, 161);
            pillFgColor = Color.rgb(166, 227, 161);
            statusBadgeText = event.getDurationMs() > 0
                    ? "DONE (exit " + event.getExitCode() + ", " + event.getDurationMs() + "ms)"
                    : "DONE (exit " + event.getExitCode() + ")";
        }

        Paint borderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        borderPaint.setStyle(Paint.Style.FILL);
        borderPaint.setColor(borderColor);
        float borderW = Math.max(4f, Math.round(7f * scale));
        canvas.drawRoundRect(
                new RectF(marginX, cardY, marginX + borderW, cardY + cardHeight),
                cornerRadius / 2f,
                cornerRadius / 2f,
                borderPaint);

        Paint pillTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(pillTextPaint, false);
        pillTextPaint.setFakeBoldText(true);
        pillTextPaint.setTextSize(13f * scale);
        pillTextPaint.setColor(pillFgColor);

        float pillPadX = Math.round(12f * scale);
        float pillW = pillTextPaint.measureText(statusBadgeText) + 2f * pillPadX;
        float pillH = Math.round(26f * scale);
        float pillX = marginX + padX;
        float pillY = cardY + Math.round(12f * scale);

        Paint pillBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        pillBgPaint.setStyle(Paint.Style.FILL);
        pillBgPaint.setColor(pillBgColor);
        canvas.drawRoundRect(
                new RectF(pillX, pillY, pillX + pillW, pillY + pillH),
                8f * scale,
                8f * scale,
                pillBgPaint);

        float pillBaselineY = pillY + pillH * 0.72f;
        canvas.drawText(statusBadgeText, pillX + pillPadX, pillBaselineY, pillTextPaint);

        Paint titlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(titlePaint, false);
        titlePaint.setFakeBoldText(true);
        titlePaint.setTextSize(14f * scale);
        titlePaint.setColor(Color.WHITE);
        canvas.drawText("ADB Command Overlay", pillX + pillW + Math.round(14f * scale), pillBaselineY, titlePaint);

        Paint tsPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(tsPaint, true);
        tsPaint.setFakeBoldText(true);
        tsPaint.setTextSize(13f * scale);
        tsPaint.setColor(Color.rgb(166, 173, 200));
        tsPaint.setTextAlign(Paint.Align.RIGHT);
        canvas.drawText(event.getTimestampText(), marginX + cardWidth - padX, pillBaselineY, tsPaint);

        float textY = cardY + headerHeight + Math.round(14f * scale);
        for (String line : cmdLines) {
            canvas.drawText(line, marginX + padX, textY, cmdPaint);
            textY += lineHeight;
        }

        if (showAll && !outLines.isEmpty()) {
            textY += interSectionGap;
            for (String line : outLines) {
                canvas.drawText(line, marginX + padX, textY, outPaint);
                textY += lineHeight;
            }
        }

        return bitmap;
    }

    @SuppressWarnings("checkstyle:MethodLength")
    private static Bitmap renderMultiHudBitmap(
            Bitmap bitmap,
            int width,
            int height,
            List<AdbOverlayEvent> events,
            boolean showAll,
            Options.AdbOverlayPosition position) {
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);

        float scale = Math.max(0.65f, Math.min(2.0f, width / 1280f));
        float marginX = Math.round(24f * scale);
        float marginBottom = Math.round(20f * scale);
        float cardWidth = width - 2f * marginX;
        float padX = Math.round(18f * scale);
        float lineHeight = Math.round(20f * scale);

        Paint cmdPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(cmdPaint, true);
        cmdPaint.setFakeBoldText(true);
        cmdPaint.setTextSize(14f * scale);
        cmdPaint.setColor(Color.rgb(137, 180, 250));

        Paint outPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(outPaint, true);
        outPaint.setFakeBoldText(true);
        outPaint.setTextSize(13f * scale);

        Paint rowBadgePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(rowBadgePaint, false);
        rowBadgePaint.setFakeBoldText(true);
        rowBadgePaint.setTextSize(11.5f * scale);

        Paint tsPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(tsPaint, true);
        tsPaint.setFakeBoldText(true);
        tsPaint.setTextSize(12.5f * scale);
        tsPaint.setColor(Color.rgb(166, 173, 200));
        tsPaint.setTextAlign(Paint.Align.RIGHT);

        float badgeColWidth = Math.round(108f * scale);
        float tsColWidth = Math.round(76f * scale);
        float cmdMaxWidth = Math.max(120f, cardWidth - 2f * padX - badgeColWidth - tsColWidth);

        List<List<String>> allCmdLines = new ArrayList<>(events.size());
        List<List<String>> allOutLines = new ArrayList<>(events.size());
        float totalRowsHeight = 0f;
        float rowVertPad = Math.round(10f * scale);

        boolean anyRunning = false;
        boolean anyError = false;

        for (AdbOverlayEvent ev : events) {
            if (ev.getStatus() == AdbOverlayEvent.Status.RUNNING) {
                anyRunning = true;
            } else if (ev.getStatus() == AdbOverlayEvent.Status.ERROR) {
                anyError = true;
            }
            List<String> cLines = wrapText(cmdPaint, "$ " + ev.getCommand(), cmdMaxWidth, 2);
            allCmdLines.add(cLines);

            List<String> oLines = new ArrayList<>();
            if (showAll) {
                String rawOut = ev.getOutput();
                String prefix;
                if (ev.getStatus() == AdbOverlayEvent.Status.ERROR) {
                    prefix = "Result (exit " + ev.getExitCode() + "): ";
                    if (rawOut == null || rawOut.isEmpty()) {
                        rawOut = "(no output)";
                    }
                } else if (ev.getStatus() == AdbOverlayEvent.Status.RUNNING) {
                    prefix = "Result: ";
                    if (rawOut == null || rawOut.isEmpty()) {
                        rawOut = "Awaiting command output...";
                    }
                } else {
                    prefix = "Result (exit " + ev.getExitCode() + "): ";
                    if (rawOut == null || rawOut.isEmpty()) {
                        rawOut = "(no output)";
                    }
                }
                oLines = wrapText(outPaint, prefix + rawOut, cmdMaxWidth + tsColWidth, 2);
            }
            allOutLines.add(oLines);

            int linesCount = Math.max(1, cLines.size()) + oLines.size();
            totalRowsHeight += linesCount * lineHeight + rowVertPad;
        }

        float headerHeight = Math.round(44f * scale);
        float bottomPad = Math.round(10f * scale);
        float cardHeight = headerHeight + totalRowsHeight + bottomPad;
        float cardY = computeCardY(height, cardHeight, marginBottom, position);

        Paint cardBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        cardBgPaint.setStyle(Paint.Style.FILL);
        cardBgPaint.setColor(Color.argb(232, 15, 17, 26));
        float cornerRadius = 14f * scale;
        RectF cardRect = new RectF(marginX, cardY, marginX + cardWidth, cardY + cardHeight);
        canvas.drawRoundRect(cardRect, cornerRadius, cornerRadius, cardBgPaint);

        int borderColor;
        int pillBgColor;
        int pillFgColor;
        String statusBadgeText;

        if (anyRunning) {
            borderColor = Color.rgb(249, 226, 175);
            pillBgColor = Color.argb(64, 249, 226, 175);
            pillFgColor = Color.rgb(249, 226, 175);
            statusBadgeText = "RUNNING (" + events.size() + " CMDS)";
        } else if (anyError) {
            borderColor = Color.rgb(243, 139, 168);
            pillBgColor = Color.argb(64, 243, 139, 168);
            pillFgColor = Color.rgb(243, 139, 168);
            statusBadgeText = "FAILED (" + events.size() + " CMDS)";
        } else {
            borderColor = Color.rgb(166, 227, 161);
            pillBgColor = Color.argb(64, 166, 227, 161);
            pillFgColor = Color.rgb(166, 227, 161);
            statusBadgeText = "DONE (" + events.size() + " CMDS)";
        }

        Paint borderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        borderPaint.setStyle(Paint.Style.FILL);
        borderPaint.setColor(borderColor);
        float borderW = Math.max(4f, Math.round(7f * scale));
        canvas.drawRoundRect(
                new RectF(marginX, cardY, marginX + borderW, cardY + cardHeight),
                cornerRadius / 2f,
                cornerRadius / 2f,
                borderPaint);

        Paint pillTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(pillTextPaint, false);
        pillTextPaint.setFakeBoldText(true);
        pillTextPaint.setTextSize(13f * scale);
        pillTextPaint.setColor(pillFgColor);

        float pillPadX = Math.round(12f * scale);
        float pillW = pillTextPaint.measureText(statusBadgeText) + 2f * pillPadX;
        float pillH = Math.round(25f * scale);
        float pillX = marginX + padX;
        float pillY = cardY + Math.round(10f * scale);

        Paint pillBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        pillBgPaint.setStyle(Paint.Style.FILL);
        pillBgPaint.setColor(pillBgColor);
        canvas.drawRoundRect(
                new RectF(pillX, pillY, pillX + pillW, pillY + pillH),
                8f * scale,
                8f * scale,
                pillBgPaint);

        float pillBaselineY = pillY + pillH * 0.72f;
        canvas.drawText(statusBadgeText, pillX + pillPadX, pillBaselineY, pillTextPaint);

        Paint titlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        applySafeTypeface(titlePaint, false);
        titlePaint.setFakeBoldText(true);
        titlePaint.setTextSize(14f * scale);
        titlePaint.setColor(Color.WHITE);
        canvas.drawText("ADB Command Overlay", pillX + pillW + Math.round(14f * scale), pillBaselineY, titlePaint);

        AdbOverlayEvent newest = events.get(events.size() - 1);
        canvas.drawText(newest.getTimestampText(), marginX + cardWidth - padX, pillBaselineY, tsPaint);

        Paint dividerPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        dividerPaint.setColor(Color.argb(28, 255, 255, 255));
        dividerPaint.setStrokeWidth(Math.max(1f, scale));

        float rowTopY = cardY + headerHeight;
        for (int i = 0; i < events.size(); i++) {
            AdbOverlayEvent ev = events.get(i);
            List<String> cLines = allCmdLines.get(i);
            List<String> oLines = allOutLines.get(i);

            canvas.drawLine(marginX + padX, rowTopY, marginX + cardWidth - padX, rowTopY, dividerPaint);

            int rowFgColor;
            int rowBgColor;
            String rowBadge;
            if (ev.getStatus() == AdbOverlayEvent.Status.RUNNING) {
                rowFgColor = Color.rgb(249, 226, 175);
                rowBgColor = Color.argb(55, 249, 226, 175);
                rowBadge = "RUNNING...";
            } else if (ev.getStatus() == AdbOverlayEvent.Status.ERROR) {
                rowFgColor = Color.rgb(243, 139, 168);
                rowBgColor = Color.argb(55, 243, 139, 168);
                rowBadge = "FAIL (exit " + ev.getExitCode() + ")";
            } else {
                rowFgColor = Color.rgb(166, 227, 161);
                rowBgColor = Color.argb(55, 166, 227, 161);
                rowBadge = "DONE (exit " + ev.getExitCode() + ")";
            }

            float badgeX = marginX + padX;
            float badgeY = rowTopY + Math.round(5f * scale);
            float badgeH = Math.round(20f * scale);
            float badgeW = Math.min(badgeColWidth - Math.round(10f * scale),
                    rowBadgePaint.measureText(rowBadge) + Math.round(16f * scale));

            pillBgPaint.setColor(rowBgColor);
            canvas.drawRoundRect(
                    new RectF(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH),
                    6f * scale,
                    6f * scale,
                    pillBgPaint);

            rowBadgePaint.setColor(rowFgColor);
            canvas.drawText(
                    rowBadge,
                    badgeX + Math.round(8f * scale),
                    badgeY + badgeH * 0.73f,
                    rowBadgePaint);

            float cmdStartX = marginX + padX + badgeColWidth;
            float lineY = rowTopY + Math.round(19f * scale);

            canvas.drawText(ev.getTimestampText(), marginX + cardWidth - padX, lineY, tsPaint);

            for (String cLine : cLines) {
                canvas.drawText(cLine, cmdStartX, lineY, cmdPaint);
                lineY += lineHeight;
            }

            if (showAll && !oLines.isEmpty()) {
                if (ev.getStatus() == AdbOverlayEvent.Status.ERROR) {
                    outPaint.setColor(Color.rgb(243, 139, 168));
                } else if (ev.getStatus() == AdbOverlayEvent.Status.RUNNING) {
                    outPaint.setColor(Color.rgb(205, 214, 244));
                } else {
                    outPaint.setColor(Color.rgb(166, 227, 161));
                }
                for (String oLine : oLines) {
                    canvas.drawText(oLine, cmdStartX, lineY, outPaint);
                    lineY += lineHeight;
                }
            }

            int linesCount = Math.max(1, cLines.size()) + oLines.size();
            rowTopY += linesCount * lineHeight + rowVertPad;
        }

        return bitmap;
    }

    static List<String> wrapText(Paint paint, String text, float maxWidth, int maxLines) {
        List<String> lines = new ArrayList<>();
        if (text == null || text.isEmpty()) {
            return lines;
        }
        String normalized = text.replace('\r', ' ').replace('\n', ' ').trim();
        String[] words = normalized.split("\\s+");
        StringBuilder currentLine = new StringBuilder();

        for (int i = 0; i < words.length; i++) {
            String word = words[i];
            String candidate = currentLine.length() == 0 ? word : currentLine + " " + word;
            if (paint.measureText(candidate) > maxWidth && currentLine.length() > 0) {
                lines.add(currentLine.toString());
                currentLine.setLength(0);
                currentLine.append(word);
                if (lines.size() >= maxLines - 1 && i < words.length - 1) {
                    for (int j = i + 1; j < words.length; j++) {
                        currentLine.append(' ').append(words[j]);
                    }
                    break;
                }
            } else {
                if (currentLine.length() > 0) {
                    currentLine.append(' ');
                }
                currentLine.append(word);
            }
        }
        if (currentLine.length() > 0) {
            lines.add(currentLine.toString());
        }
        return lines;
    }
}
