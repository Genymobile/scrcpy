package com.genymobile.scrcpy.android;

import android.content.Context;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

final class ScrcpySurfaceView extends SurfaceView implements SurfaceHolder.Callback {
    interface TouchListener {
        void onTouch(int action, float x, float y, float pressure, int width, int height);
    }

    interface SurfaceListener {
        void onSurfaceCreated();
        void onSurfaceDestroyed();
    }

    private TouchListener touchListener;
    private SurfaceListener surfaceListener;
    private int videoWidth;
    private int videoHeight;

    ScrcpySurfaceView(Context context) {
        super(context);
        setZOrderMediaOverlay(true);
        getHolder().addCallback(this);
        setFocusable(true);
    }

    void setTouchListener(TouchListener touchListener) {
        this.touchListener = touchListener;
    }

    void setSurfaceListener(SurfaceListener surfaceListener) {
        this.surfaceListener = surfaceListener;
    }

    boolean isReady() {
        return getHolder().getSurface().isValid();
    }

    SurfaceHolder holder() {
        return getHolder();
    }

    void setVideoSize(int width, int height) {
        videoWidth = width;
        videoHeight = height;
        requestLayout();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);
        if (videoWidth > 0 && videoHeight > 0 && width > 0 && height > 0) {
            if ((long) width * videoHeight > (long) height * videoWidth) {
                width = height * videoWidth / videoHeight;
            } else {
                height = width * videoHeight / videoWidth;
            }
        }
        setMeasuredDimension(width, height);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (touchListener == null || videoWidth == 0 || videoHeight == 0) {
            return true;
        }
        int action = event.getActionMasked();
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_UP
                && action != MotionEvent.ACTION_MOVE) {
            return true;
        }
        float x = event.getX() * videoWidth / Math.max(1, getWidth());
        float y = event.getY() * videoHeight / Math.max(1, getHeight());
        float pressure = event.getPressure();
        touchListener.onTouch(action, x, y, pressure, videoWidth, videoHeight);
        if (action == MotionEvent.ACTION_UP) {
            performClick();
        }
        return true;
    }

    @Override
    public boolean performClick() {
        super.performClick();
        return true;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (surfaceListener != null) {
            surfaceListener.onSurfaceCreated();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (surfaceListener != null) {
            surfaceListener.onSurfaceDestroyed();
        }
    }
}
