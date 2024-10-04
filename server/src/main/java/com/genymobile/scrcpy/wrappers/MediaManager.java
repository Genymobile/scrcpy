package com.genymobile.scrcpy.wrappers;

import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;

import com.genymobile.scrcpy.FakeContext;
import com.genymobile.scrcpy.util.Ln;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

public class MediaManager {
    final static byte KEY_DURATION = 0;
    final static byte KEY_ALBUM = 1;
    final static byte KEY_ARTIST = 2;
    final static byte KEY_TITLE = 3;

    final static byte STATE_PLAYING = 0;
    final static byte STATE_STOPPED = 1;
    final static byte STATE_PAUSED = 2;

    MediaSessionManager sessionManager;
    private MediaChange mediaChangeListener;
    boolean started = false;

    public interface MediaChange {
        void onMetadataChange(int id, MediaMetadata metadata);
        void onPlaybackStateChange(int id, PlaybackState playbackState);
        void onRemove(int id);
    }
    public static MediaManager create() {
        MediaSessionManager manager = FakeContext.get().getSystemService(MediaSessionManager.class);
        return new MediaManager(manager);
    }

    int nextId = 0;
    HashMap<String, Integer> idMap = new HashMap<>();
    List<MediaController> mediaControllers = Collections.emptyList();


    private MediaManager(MediaSessionManager sessionManager) {
        this.sessionManager = sessionManager;

    }

    public void setMediaChangeListener(MediaChange listener) {
        this.mediaChangeListener = listener;
    }

    public void start() {
        if (started) {
            return;
        }

        sessionManager.addOnActiveSessionsChangedListener(new MediaSessionManager.OnActiveSessionsChangedListener() {
            @Override
            public void onActiveSessionsChanged(List<MediaController> controllers) {
                Ln.i("MediaManager: Active Sessions changed");
                if (controllers == null) {
                    controllers = Collections.emptyList();
                }
                // add
                for(MediaController controller : controllers) {
                    if (!mediaControllers.contains(controller)) {
                        addMediaController(controller);
                    }
                }
                for(MediaController controller: mediaControllers) {
                    if (!controllers.contains(controller)) {
                        removeMediaController(controller);
                    }
                }

                mediaControllers = new ArrayList<>(controllers);
            }
        }, null);

        mediaControllers = sessionManager.getActiveSessions(null);
        for (MediaController controller : mediaControllers) {
            addMediaController(controller);
        }

        started = true;
    }

    public static byte[] mediaMetadataSerialize(MediaMetadata metadata) {
        ArrayList<Byte> payload = new ArrayList<Byte>();
        for (String key : metadata.keySet()) {
            byte field_id;
            byte[] field_value;
            switch(key) {
                case MediaMetadata.METADATA_KEY_DURATION:
                    field_id = KEY_DURATION;
                    field_value = (""+metadata.getLong(key)).getBytes();
                    break;
                case MediaMetadata.METADATA_KEY_ALBUM:
                    field_id = KEY_ALBUM;
                    field_value = metadata.getString(key).getBytes();
                    break;
                case MediaMetadata.METADATA_KEY_ARTIST:
                    field_id = KEY_ARTIST;
                    field_value = metadata.getString(key).getBytes();
                    break;
                case MediaMetadata.METADATA_KEY_TITLE:
                    field_id = KEY_TITLE;
                    field_value = metadata.getString(key).getBytes();
                    break;
                default:
                    field_id = 0;
                    field_value = null;
            }

            if (field_value != null) {
                payload.add(field_id);
                for (byte b : field_value) {
                    payload.add(b);
                }
                payload.add((byte)0);
            }


        }
        byte[] result = new byte[payload.size()];
        for (int i = 0; i < payload.size(); i++) {
            result[i] = payload.get(i);
        }
        return result;
    }

    public int playbackStateSerialize(PlaybackState state) {
        switch(state.getState()) {
            case PlaybackState.STATE_PLAYING:
                return STATE_PLAYING;
            case PlaybackState.STATE_STOPPED:
                return STATE_STOPPED;
            case PlaybackState.STATE_PAUSED:
                return STATE_PAUSED;
            default:
                return -1;
        }
    }

    private void removeMediaController(MediaController controller) {
        int controllerId = findId(controller);
        Ln.i("Remove MediaController ID:" + controllerId + " pkg:" + controller.getPackageName());
        mediaChangeListener.onRemove(controllerId);
    }

    private int findId(MediaController controller) {
        String packageName = controller.getPackageName();
        Integer id = this.idMap.get(packageName);
        if (id == null) {
            id = nextId;
            nextId++;
            this.idMap.put(packageName, id);
        }

        return id;
    }

    private void addMediaController(MediaController controller) {
        final int controllerId = findId(controller);
        Ln.i("New MediaController ID:" + controllerId + " pkg:" + controller.getPackageName());
        controller.registerCallback(new MediaController.Callback() {
            @Override
            public void onMetadataChanged(MediaMetadata metadata) {
                super.onMetadataChanged(metadata);
                Ln.i("MediaController metadata change " + controllerId);
                mediaChangeListener.onMetadataChange(controllerId, metadata);
            }

            @Override
            public void onPlaybackStateChanged(PlaybackState state) {
                super.onPlaybackStateChanged(state);
                Ln.i("MediaController playstate change " + controllerId);
                mediaChangeListener.onPlaybackStateChange(controllerId, state);
            }
        });
        mediaChangeListener.onMetadataChange(controllerId, controller.getMetadata());
        mediaChangeListener.onPlaybackStateChange(controllerId, controller.getPlaybackState());
    }
}
