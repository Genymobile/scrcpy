package com.genymobile.scrcpy;

import android.media.MediaCodec;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import java.nio.ByteBuffer;

import static android.media.MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED;
import static android.os.Build.VERSION.SDK_INT;

/**
 * Version of {@link MediaCodec} that backports {@link #getOutputBuffer} to Kitkat.
 * The backported implementation isn't thread safe.
 */
abstract class MediaCodecCompat {
    private final MediaCodec delegate;

    MediaCodecCompat(@NonNull MediaCodec delegate) {
        this.delegate = delegate;
    }

    @NonNull
    protected MediaCodec getDelegate() {
        return delegate;
    }

    @NonNull
    static MediaCodecCompat wrap(@NonNull MediaCodec codec) {
        if (SDK_INT >= 21) {
            return new Platform(codec);
        } else {
            return new Backport(codec);
        }
    }

    abstract int dequeueOutputBuffer(
            @NonNull MediaCodec.BufferInfo info, long timeoutUs);

    @Nullable
    abstract ByteBuffer getOutputBuffer(int index);

    abstract void releaseOutputBuffer(int index, boolean render);

    @SuppressWarnings("deprecation")
    private static class Backport extends MediaCodecCompat {
        private ByteBuffer[] cachedOutputBuffers = null;

        Backport(@NonNull MediaCodec delegate) {
            super(delegate);
        }

        @Override
        int dequeueOutputBuffer(
                @NonNull MediaCodec.BufferInfo info, long timeoutUs) {
            final int res = getDelegate().dequeueOutputBuffer(info, timeoutUs);
            if (res == INFO_OUTPUT_BUFFERS_CHANGED) {
                cachedOutputBuffers = null;
            }
            return res;
        }

        @Nullable
        @Override
        ByteBuffer getOutputBuffer(int index) {
            if (cachedOutputBuffers == null) {
                cacheOutputBuffers();
            }
            if (cachedOutputBuffers == null) {
                throw new IllegalStateException();
            }
            return cachedOutputBuffers[index];
        }

        @Override
        void releaseOutputBuffer(int index, boolean render) {
            cachedOutputBuffers = null;
            getDelegate().releaseOutputBuffer(index, render);
        }

        private void cacheOutputBuffers() {
            ByteBuffer[] buffers = null;
            try {
                buffers = getDelegate().getOutputBuffers();
            } catch (IllegalStateException e) {
                // we don't get buffers in async mode
            }
            cachedOutputBuffers = buffers;
        }
    }

    @RequiresApi(21)
    private static class Platform extends MediaCodecCompat {
        Platform(@NonNull MediaCodec delegate) {
            super(delegate);
        }

        @Override
        int dequeueOutputBuffer(
                @NonNull MediaCodec.BufferInfo info, long timeoutUs) {
            return getDelegate().dequeueOutputBuffer(info, timeoutUs);
        }

        @Nullable
        @Override
        ByteBuffer getOutputBuffer(int index) {
            return getDelegate().getOutputBuffer(index);
        }

        @Override
        void releaseOutputBuffer(int index, boolean render) {
            getDelegate().releaseOutputBuffer(index, render);
        }
    }
}
