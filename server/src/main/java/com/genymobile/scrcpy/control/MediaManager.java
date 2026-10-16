package com.genymobile.scrcpy.control;

import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;

import android.content.Context;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

import java.util.List;

// Forward the media session of the device (metadata and playback state) to the client, and execute the media actions it requests.
// Everything runs on a dedicated handler thread.
public final class MediaManager {

    // Values must be kept in sync with the client (device_msg.h and control_msg.h)
    private static final int STATE_STOPPED = 0;
    private static final int STATE_PLAYING = 1;
    private static final int STATE_PAUSED = 2;

    private static final int CAP_PLAY = 1 << 0;
    private static final int CAP_PAUSE = 1 << 1;
    private static final int CAP_NEXT = 1 << 2;
    private static final int CAP_PREVIOUS = 1 << 3;
    private static final int CAP_SEEK = 1 << 4;

    private static final int ACTION_PLAY = 0;
    private static final int ACTION_PAUSE = 1;
    private static final int ACTION_STOP = 2;
    private static final int ACTION_NEXT = 3;
    private static final int ACTION_PREVIOUS = 4;
    private static final int ACTION_SEEK = 5;

    private static final long POSITION_UNKNOWN = -1;
    private static final long DURATION_UNKNOWN = -1;

    private final DeviceMessageSender sender;
    private final HandlerThread thread = new HandlerThread("MediaManager");
    private Handler handler;

    // The media session exposed to the client (the one with the highest priority), null if none
    private MediaController current;

    private final MediaController.Callback callback = new MediaController.Callback() {
        @Override
        public void onMetadataChanged(MediaMetadata metadata) {
            sendUpdate();
        }

        @Override
        public void onPlaybackStateChanged(PlaybackState state) {
            sendUpdate();
        }
    };

    public MediaManager(DeviceMessageSender sender) {
        this.sender = sender;
    }

    public void start() {
        thread.start();
        handler = new Handler(thread.getLooper());
        handler.post(this::init);
    }

    public void stop() {
        thread.quit();
    }

    public void handleAction(int action, long positionMs) {
        handler.post(() -> performAction(action, positionMs));
    }

    private void init() {
        try {
            MediaSessionManager manager = (MediaSessionManager) FakeContext.get().getSystemService(Context.MEDIA_SESSION_SERVICE);
            manager.addOnActiveSessionsChangedListener(this::onActiveSessionsChanged, null, handler);
            onActiveSessionsChanged(manager.getActiveSessions(null));
        } catch (RuntimeException e) {
            // SecurityException if the permission is missing (or any unexpected failure)
            Ln.e("Could not access media sessions, media controls will not work", e);
        }
    }

    private void onActiveSessionsChanged(List<MediaController> controllers) {
        // The list is sorted by priority: the first session is the one controlled by the media keys of the device
        MediaController first = controllers.isEmpty() ? null : controllers.get(0);
        if (current != null && first != null && current.getSessionToken().equals(first.getSessionToken())) {
            // Same session (the callback is still registered)
            return;
        }

        if (current != null) {
            current.unregisterCallback(callback);
        }
        current = first;
        if (current != null) {
            current.registerCallback(callback, handler);
            Ln.d("Media session: " + current.getPackageName());
        } else {
            Ln.d("Media session: none");
        }

        sendUpdate();
    }

    private void sendUpdate() {
        if (current == null) {
            sender.send(DeviceMessage.createMediaUpdate(STATE_STOPPED, POSITION_UNKNOWN, 0, DURATION_UNKNOWN, null, null, null));
            return;
        }

        int state = STATE_STOPPED;
        long position = POSITION_UNKNOWN;
        int capabilities = 0;
        PlaybackState playbackState = current.getPlaybackState();
        if (playbackState != null) {
            state = convertState(playbackState.getState());
            position = playbackState.getPosition();
            if (position < 0) {
                position = POSITION_UNKNOWN;
            } else if (state == STATE_PLAYING) {
                // Extrapolate the position to the current time
                long elapsed = SystemClock.elapsedRealtime() - playbackState.getLastPositionUpdateTime();
                position += (long) (elapsed * playbackState.getPlaybackSpeed());
            }
            capabilities = convertCapabilities(playbackState.getActions());
        }

        long duration = DURATION_UNKNOWN;
        String title = null;
        String artist = null;
        String album = null;
        MediaMetadata metadata = current.getMetadata();
        if (metadata != null) {
            if (metadata.getLong(MediaMetadata.METADATA_KEY_DURATION) > 0) {
                duration = metadata.getLong(MediaMetadata.METADATA_KEY_DURATION);
            }
            title = getFirstString(metadata, MediaMetadata.METADATA_KEY_TITLE, MediaMetadata.METADATA_KEY_DISPLAY_TITLE);
            artist = getFirstString(metadata, MediaMetadata.METADATA_KEY_ARTIST, MediaMetadata.METADATA_KEY_ALBUM_ARTIST,
                    MediaMetadata.METADATA_KEY_DISPLAY_SUBTITLE);
            album = getFirstString(metadata, MediaMetadata.METADATA_KEY_ALBUM);
        }

        sender.send(DeviceMessage.createMediaUpdate(state, position, capabilities, duration, title, artist, album));
    }

    private static int convertState(int state) {
        switch (state) {
            case PlaybackState.STATE_PLAYING:
            case PlaybackState.STATE_BUFFERING:
            case PlaybackState.STATE_CONNECTING:
            case PlaybackState.STATE_FAST_FORWARDING:
            case PlaybackState.STATE_REWINDING:
            case PlaybackState.STATE_SKIPPING_TO_NEXT:
            case PlaybackState.STATE_SKIPPING_TO_PREVIOUS:
            case PlaybackState.STATE_SKIPPING_TO_QUEUE_ITEM:
                return STATE_PLAYING;
            case PlaybackState.STATE_PAUSED:
                return STATE_PAUSED;
            default:
                // STATE_NONE, STATE_STOPPED, STATE_ERROR
                return STATE_STOPPED;
        }
    }

    private static int convertCapabilities(long actions) {
        int caps = 0;
        if ((actions & (PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PLAY_PAUSE)) != 0) {
            caps |= CAP_PLAY;
        }
        if ((actions & (PlaybackState.ACTION_PAUSE | PlaybackState.ACTION_PLAY_PAUSE)) != 0) {
            caps |= CAP_PAUSE;
        }
        if ((actions & PlaybackState.ACTION_SKIP_TO_NEXT) != 0) {
            caps |= CAP_NEXT;
        }
        if ((actions & PlaybackState.ACTION_SKIP_TO_PREVIOUS) != 0) {
            caps |= CAP_PREVIOUS;
        }
        if ((actions & PlaybackState.ACTION_SEEK_TO) != 0) {
            caps |= CAP_SEEK;
        }
        return caps;
    }

    // Return the first non-empty string for the given keys, or null
    private static String getFirstString(MediaMetadata metadata, String... keys) {
        for (String key : keys) {
            String value = metadata.getString(key);
            if (value != null && !value.isEmpty()) {
                return value;
            }
        }
        return null;
    }

    private void performAction(int action, long positionMs) {
        if (current == null) {
            Ln.d("No media session, action ignored");
            return;
        }

        MediaController.TransportControls controls = current.getTransportControls();
        switch (action) {
            case ACTION_PLAY:
                controls.play();
                break;
            case ACTION_PAUSE:
                controls.pause();
                break;
            case ACTION_STOP:
                controls.stop();
                break;
            case ACTION_NEXT:
                controls.skipToNext();
                break;
            case ACTION_PREVIOUS:
                controls.skipToPrevious();
                break;
            case ACTION_SEEK:
                controls.seekTo(positionMs);
                break;
            default:
                Ln.w("Unknown media action: " + action);
                break;
        }
    }
}
