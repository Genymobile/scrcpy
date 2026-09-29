#include "recording_manager.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libavcodec/avcodec.h>
#include <SDL3/SDL_filesystem.h>

#include "events.h"
#include "recorder.h"
#include "util/log.h"

#define DOWNCAST_VIDEO(SINK) \
    container_of(SINK, struct sc_recording_manager, video_packet_sink)
#define DOWNCAST_AUDIO(SINK) \
    container_of(SINK, struct sc_recording_manager, audio_packet_sink)

#define SC_RECORDING_KEYFRAME_TIMEOUT SC_TICK_FROM_SEC(12)

struct sc_recording_session {
    struct sc_recording_manager *manager;
    struct sc_recorder recorder;
    bool recorder_initialized;
    bool recorder_started;
    bool direct;
    bool discard_on_end;
    bool forced_failure;
    bool success;
    bool suppress_event;
};

static void
sc_recording_manager_post_state(struct sc_recording_manager *manager) {
    sc_push_event_with_data(SC_EVENT_RECORDING_STATE_CHANGED, manager);
}

static void
sc_recording_manager_set_state_locked(
    struct sc_recording_manager *manager, enum sc_recording_state state,
    enum sc_recording_notification notification) {
    sc_mutex_assert(&manager->mutex);
    manager->state = state;
    if (notification != SC_RECORDING_NOTIFICATION_NONE) {
        manager->notification = notification;
    }
}

static void
sc_recording_manager_on_save_status(
    struct sc_recording_save *save, enum sc_recording_save_status status,
    void *userdata) {
    (void) save;
    struct sc_recording_manager *manager = userdata;
    enum sc_recording_state state;
    enum sc_recording_notification notification =
        SC_RECORDING_NOTIFICATION_NONE;
    switch (status) {
        case SC_RECORDING_SAVE_STATUS_AWAITING:
            state = SC_RECORDING_STATE_AWAITING_SAVE;
            break;
        case SC_RECORDING_SAVE_STATUS_SAVING:
            state = SC_RECORDING_STATE_SAVING;
            break;
        case SC_RECORDING_SAVE_STATUS_SAVED:
            state = SC_RECORDING_STATE_IDLE;
            notification = SC_RECORDING_NOTIFICATION_SAVED;
            break;
        case SC_RECORDING_SAVE_STATUS_FAILED:
            state = SC_RECORDING_STATE_ERROR;
            notification = SC_RECORDING_NOTIFICATION_FAILED;
            break;
        case SC_RECORDING_SAVE_STATUS_DISCARDED:
            state = SC_RECORDING_STATE_IDLE;
            break;
        default:
            assert(false);
            return;
    }

    sc_mutex_lock(&manager->mutex);
    sc_recording_manager_set_state_locked(manager, state, notification);
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
}

static AVPacket *
sc_recording_packet_clone(const AVPacket *packet) {
    AVPacket *copy = av_packet_alloc();
    if (!copy) {
        return NULL;
    }
    if (av_packet_ref(copy, packet) < 0) {
        av_packet_free(&copy);
        return NULL;
    }
    return copy;
}

static AVCodecParameters *
sc_recording_codec_parameters_clone(const AVCodecParameters *params) {
    AVCodecParameters *copy = avcodec_parameters_alloc();
    if (!copy) {
        return NULL;
    }

    if (avcodec_parameters_copy(copy, params) < 0) {
        avcodec_parameters_free(&copy);
        return NULL;
    }

    return copy;
}

static void
sc_recording_session_on_ended(struct sc_recorder *recorder, bool success,
                              void *userdata) {
    (void) recorder;
    struct sc_recording_session *session = userdata;
    session->success = success && !session->forced_failure;
    if (!session->suppress_event) {
        sc_push_event_with_data(SC_EVENT_RECORDING_SESSION_ENDED, session);
    }
}

static bool
sc_recording_manager_create_session_locked(
    struct sc_recording_manager *manager) {
    sc_mutex_assert(&manager->mutex);
    assert(!manager->session);
    assert(manager->pending_filename);
    assert(!manager->video_requested || manager->video_params);

    struct sc_recording_session *session = calloc(1, sizeof(*session));
    if (!session) {
        LOG_OOM();
        return false;
    }
    session->manager = manager;
    session->direct = manager->pending_direct;

    static const struct sc_recorder_callbacks recorder_cbs = {
        .on_ended = sc_recording_session_on_ended,
    };
    bool video = manager->video_requested && manager->video_params;
    bool audio = manager->audio_requested && manager->audio_available
              && manager->audio_params;
    if (!sc_recorder_init(&session->recorder, manager->pending_filename,
                          manager->pending_format, video, audio,
                          manager->orientation, &recorder_cbs, session)) {
        free(session);
        return false;
    }
    session->recorder_initialized = true;
    if (!sc_recorder_start(&session->recorder)) {
        sc_recorder_destroy(&session->recorder);
        free(session);
        return false;
    }
    session->recorder_started = true;

    if (video && !session->recorder.video_packet_sink.ops->open(
            &session->recorder.video_packet_sink, manager->video_codec,
            manager->video_params, NULL)) {
        goto error_stop_session;
    }
    if (audio && !session->recorder.audio_packet_sink.ops->open(
            &session->recorder.audio_packet_sink, manager->audio_codec,
            manager->audio_params, NULL)) {
        goto error_stop_session;
    }

    manager->session = session;
    if (video && manager->video_config
            && !session->recorder.video_packet_sink.ops->push(
                &session->recorder.video_packet_sink,
                manager->video_config)) {
        goto error_stop_session;
    }
    if (audio && manager->audio_config
            && !session->recorder.audio_packet_sink.ops->push(
                &session->recorder.audio_packet_sink,
                manager->audio_config)) {
        goto error_stop_session;
    }

    if (video && manager->pending_video_keyframe) {
        sc_recording_manager_set_state_locked(
            manager, SC_RECORDING_STATE_RECORDING,
            SC_RECORDING_NOTIFICATION_STARTED);
        if (!session->recorder.video_packet_sink.ops->push(
                &session->recorder.video_packet_sink,
                manager->pending_video_keyframe)) {
            goto error_stop_session;
        }
        av_packet_free(&manager->pending_video_keyframe);
    }

    manager->prepare_deadline = video ? sc_tick_now()
                                      + SC_RECORDING_KEYFRAME_TIMEOUT : 0;
    free(manager->pending_filename);
    manager->pending_filename = NULL;
    return true;

error_stop_session:
    session->suppress_event = true;
    sc_recorder_stop(&session->recorder);
    sc_recorder_join(&session->recorder);
    sc_recorder_destroy(&session->recorder);
    if (manager->session == session) {
        manager->session = NULL;
    }
    free(session);
    return false;
}

static bool
sc_recording_manager_maybe_create_session_locked(
    struct sc_recording_manager *manager) {
    sc_mutex_assert(&manager->mutex);
    if (manager->session || !manager->pending_filename
            || (manager->video_requested && !manager->video_ready)
            || (manager->audio_requested && !manager->audio_ready)) {
        return true;
    }
    return sc_recording_manager_create_session_locked(manager);
}

static bool
sc_recording_manager_request_locked(struct sc_recording_manager *manager,
                                    char *filename,
                                    enum sc_record_format format,
                                    bool direct) {
    sc_mutex_assert(&manager->mutex);
    if (manager->state != SC_RECORDING_STATE_IDLE || manager->session
            || manager->pending_filename || manager->streams_closed) {
        free(filename);
        return false;
    }
    manager->pending_filename = filename;
    av_packet_free(&manager->pending_video_keyframe);
    manager->pending_format = format;
    manager->pending_direct = direct;
    sc_recording_manager_set_state_locked(
        manager, SC_RECORDING_STATE_PREPARING,
        SC_RECORDING_NOTIFICATION_NONE);
    if (!sc_recording_manager_maybe_create_session_locked(manager)) {
        free(manager->pending_filename);
        manager->pending_filename = NULL;
        sc_recording_manager_set_state_locked(
            manager, SC_RECORDING_STATE_ERROR,
            SC_RECORDING_NOTIFICATION_FAILED);
        return false;
    }
    return true;
}

static char *
sc_recording_build_name(const char *directory) {
    time_t now = time(NULL);
    struct tm local;
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[32];
    if (!strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local)) {
        return NULL;
    }
    char *path = NULL;
    if (asprintf(&path, "%sscrcpy-%s.mp4", directory, stamp) == -1) {
        return NULL;
    }
    return path;
}

static char *
sc_recording_create_temp_path(void) {
    char *pref = SDL_GetPrefPath("Genymobile", "scrcpy");
    if (!pref) {
        LOGE("Could not get recording directory: %s", SDL_GetError());
        return NULL;
    }
    char *directory = NULL;
    if (asprintf(&directory, "%srecordings/", pref) == -1) {
        SDL_free(pref);
        return NULL;
    }
    SDL_free(pref);
    if (!SDL_CreateDirectory(directory)) {
        LOGE("Could not create recording directory %s: %s", directory,
             SDL_GetError());
        free(directory);
        return NULL;
    }
    char *path = sc_recording_build_name(directory);
    free(directory);
    return path;
}

static bool
sc_recording_manager_video_open(struct sc_packet_sink *sink,
                                const AVCodec *codec,
                                const AVCodecParameters *params,
                                const struct sc_stream_session *session) {
    (void) session;
    struct sc_recording_manager *manager = DOWNCAST_VIDEO(sink);
    AVCodecParameters *copy =
        sc_recording_codec_parameters_clone(params);
    if (!copy) {
        LOG_OOM();
        return false;
    }
    sc_mutex_lock(&manager->mutex);
    avcodec_parameters_free(&manager->video_params);
    manager->video_codec = codec;
    manager->video_params = copy;
    manager->video_ready = true;
    bool ok = sc_recording_manager_maybe_create_session_locked(manager);
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
    return ok;
}

static bool
sc_recording_manager_audio_open(struct sc_packet_sink *sink,
                                const AVCodec *codec,
                                const AVCodecParameters *params,
                                const struct sc_stream_session *session) {
    (void) session;
    struct sc_recording_manager *manager = DOWNCAST_AUDIO(sink);
    AVCodecParameters *copy =
        sc_recording_codec_parameters_clone(params);
    if (!copy) {
        LOG_OOM();
        return false;
    }
    sc_mutex_lock(&manager->mutex);
    avcodec_parameters_free(&manager->audio_params);
    manager->audio_codec = codec;
    manager->audio_params = copy;
    manager->audio_ready = true;
    manager->audio_available = true;
    bool ok = sc_recording_manager_maybe_create_session_locked(manager);
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
    return ok;
}

static void
sc_recording_manager_stop_locked(struct sc_recording_manager *manager,
                                 bool discard, bool forced_failure) {
    sc_mutex_assert(&manager->mutex);
    struct sc_recording_session *session = manager->session;
    if (!session) {
        free(manager->pending_filename);
        manager->pending_filename = NULL;
        av_packet_free(&manager->pending_video_keyframe);
        sc_recording_manager_set_state_locked(
            manager, forced_failure ? SC_RECORDING_STATE_ERROR
                                    : SC_RECORDING_STATE_IDLE,
            forced_failure ? SC_RECORDING_NOTIFICATION_FAILED
                           : SC_RECORDING_NOTIFICATION_NONE);
        return;
    }
    session->discard_on_end = discard;
    session->forced_failure = forced_failure;
    sc_recording_manager_set_state_locked(
        manager, SC_RECORDING_STATE_FINALIZING,
        SC_RECORDING_NOTIFICATION_NONE);
    sc_recorder_stop(&session->recorder);
}

static void
sc_recording_manager_video_close(struct sc_packet_sink *sink) {
    struct sc_recording_manager *manager = DOWNCAST_VIDEO(sink);
    sc_mutex_lock(&manager->mutex);
    manager->streams_closed = true;
    if (manager->state == SC_RECORDING_STATE_PREPARING
            || manager->state == SC_RECORDING_STATE_RECORDING) {
        sc_recording_manager_stop_locked(manager, false, false);
    }
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
}

static void
sc_recording_manager_audio_close(struct sc_packet_sink *sink) {
    struct sc_recording_manager *manager = DOWNCAST_AUDIO(sink);
    sc_mutex_lock(&manager->mutex);
    manager->audio_available = false;
    if (!manager->video_requested
            && (manager->state == SC_RECORDING_STATE_PREPARING
                || manager->state == SC_RECORDING_STATE_RECORDING)) {
        sc_recording_manager_stop_locked(manager, false, false);
    }
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
}

static bool
sc_recording_manager_video_push(struct sc_packet_sink *sink,
                                const AVPacket *packet) {
    struct sc_recording_manager *manager = DOWNCAST_VIDEO(sink);
    bool state_changed = false;
    bool ok = true;
    sc_mutex_lock(&manager->mutex);

    if (packet->pts == AV_NOPTS_VALUE) {
        AVPacket *copy = sc_recording_packet_clone(packet);
        if (!copy) {
            ok = false;
            goto end;
        }
        av_packet_free(&manager->video_config);
        manager->video_config = copy;
        if (manager->session) {
            ok = manager->session->recorder.video_packet_sink.ops->push(
                &manager->session->recorder.video_packet_sink, packet);
        }
        goto end;
    }

    if (!manager->session) {
        if (manager->state == SC_RECORDING_STATE_PREPARING
                && (packet->flags & AV_PKT_FLAG_KEY)
                && !manager->pending_video_keyframe) {
            manager->pending_video_keyframe =
                sc_recording_packet_clone(packet);
            if (!manager->pending_video_keyframe) {
                ok = false;
            }
        }
        goto end;
    }
    if (manager->state == SC_RECORDING_STATE_PREPARING) {
        if (!(packet->flags & AV_PKT_FLAG_KEY)) {
            if (!manager->control
                    && sc_tick_now() >= manager->prepare_deadline) {
                LOGE("Timed out waiting for a recording keyframe");
                sc_recording_manager_stop_locked(manager, true, true);
                state_changed = true;
            }
            goto end;
        }
        sc_recording_manager_set_state_locked(
            manager, SC_RECORDING_STATE_RECORDING,
            SC_RECORDING_NOTIFICATION_STARTED);
        state_changed = true;
    }
    if (manager->state == SC_RECORDING_STATE_RECORDING) {
        ok = manager->session->recorder.video_packet_sink.ops->push(
            &manager->session->recorder.video_packet_sink, packet);
    }

end:
    sc_mutex_unlock(&manager->mutex);
    if (state_changed) {
        sc_recording_manager_post_state(manager);
    }
    return ok;
}

static bool
sc_recording_manager_audio_push(struct sc_packet_sink *sink,
                                const AVPacket *packet) {
    struct sc_recording_manager *manager = DOWNCAST_AUDIO(sink);
    bool ok = true;
    sc_mutex_lock(&manager->mutex);
    if (packet->pts == AV_NOPTS_VALUE) {
        AVPacket *copy = sc_recording_packet_clone(packet);
        if (!copy) {
            ok = false;
            goto end;
        }
        av_packet_free(&manager->audio_config);
        manager->audio_config = copy;
        if (manager->session) {
            ok = manager->session->recorder.audio_packet_sink.ops->push(
                &manager->session->recorder.audio_packet_sink, packet);
        }
    } else if (manager->session) {
        if (!manager->video_requested
                && manager->state == SC_RECORDING_STATE_PREPARING) {
            sc_recording_manager_set_state_locked(
                manager, SC_RECORDING_STATE_RECORDING,
                SC_RECORDING_NOTIFICATION_STARTED);
            sc_recording_manager_post_state(manager);
        }
        if (manager->state == SC_RECORDING_STATE_RECORDING) {
            ok = manager->session->recorder.audio_packet_sink.ops->push(
                &manager->session->recorder.audio_packet_sink, packet);
        }
    }
end:
    sc_mutex_unlock(&manager->mutex);
    return ok;
}

static void
sc_recording_manager_audio_disable(struct sc_packet_sink *sink) {
    struct sc_recording_manager *manager = DOWNCAST_AUDIO(sink);
    sc_mutex_lock(&manager->mutex);
    manager->audio_ready = true;
    manager->audio_available = false;
    bool ok = sc_recording_manager_maybe_create_session_locked(manager);
    if (!ok) {
        sc_recording_manager_set_state_locked(
            manager, SC_RECORDING_STATE_ERROR,
            SC_RECORDING_NOTIFICATION_FAILED);
    }
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
}

bool
sc_recording_manager_init(struct sc_recording_manager *manager,
                          bool dynamic_enabled, bool control, bool video,
                          bool audio, enum sc_orientation orientation) {
    memset(manager, 0, sizeof(*manager));
    if (!sc_mutex_init(&manager->mutex)) {
        return false;
    }
    manager->initialized = true;
    manager->dynamic_enabled = dynamic_enabled;
    manager->control = control;
    manager->video_requested = video;
    manager->audio_requested = audio;
    manager->audio_ready = !audio;
    manager->audio_available = audio;
    manager->orientation = orientation;
    manager->state = SC_RECORDING_STATE_IDLE;
    static const struct sc_recording_save_callbacks save_cbs = {
        .on_status_changed = sc_recording_manager_on_save_status,
    };
    sc_recording_save_init(&manager->save, &save_cbs, manager);

    static const struct sc_packet_sink_ops video_ops = {
        .open = sc_recording_manager_video_open,
        .close = sc_recording_manager_video_close,
        .push = sc_recording_manager_video_push,
    };
    static const struct sc_packet_sink_ops audio_ops = {
        .open = sc_recording_manager_audio_open,
        .close = sc_recording_manager_audio_close,
        .push = sc_recording_manager_audio_push,
        .disable = sc_recording_manager_audio_disable,
    };
    manager->video_packet_sink.ops = &video_ops;
    manager->audio_packet_sink.ops = &audio_ops;
    return true;
}

bool
sc_recording_manager_start_initial(struct sc_recording_manager *manager,
                                   const char *filename,
                                   enum sc_record_format format) {
    char *copy = strdup(filename);
    if (!copy) {
        LOG_OOM();
        return false;
    }
    sc_mutex_lock(&manager->mutex);
    bool ok = sc_recording_manager_request_locked(manager, copy, format, true);
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
    return ok;
}

bool
sc_recording_manager_start(struct sc_recording_manager *manager) {
    char *path = sc_recording_create_temp_path();
    if (!path) {
        return false;
    }
    sc_mutex_lock(&manager->mutex);
    bool ok = manager->dynamic_enabled
           && sc_recording_manager_request_locked(
                  manager, path, SC_RECORD_FORMAT_MP4, false);
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
    return ok;
}

bool
sc_recording_manager_stop(struct sc_recording_manager *manager) {
    sc_mutex_lock(&manager->mutex);
    if (manager->state != SC_RECORDING_STATE_PREPARING
            && manager->state != SC_RECORDING_STATE_RECORDING) {
        sc_mutex_unlock(&manager->mutex);
        return false;
    }
    bool discard = manager->state == SC_RECORDING_STATE_PREPARING;
    sc_recording_manager_stop_locked(manager, discard, false);
    sc_mutex_unlock(&manager->mutex);
    sc_recording_manager_post_state(manager);
    return true;
}

enum sc_recording_state
sc_recording_manager_get_state(struct sc_recording_manager *manager) {
    sc_mutex_lock(&manager->mutex);
    enum sc_recording_state state = manager->state;
    sc_mutex_unlock(&manager->mutex);
    return state;
}

bool
sc_recording_manager_is_available(struct sc_recording_manager *manager) {
    sc_mutex_lock(&manager->mutex);
    bool available = manager->dynamic_enabled && manager->video_ready
                  && !manager->streams_closed
                  && (manager->state == SC_RECORDING_STATE_IDLE
                      || manager->state == SC_RECORDING_STATE_PREPARING
                      || manager->state == SC_RECORDING_STATE_RECORDING);
    sc_mutex_unlock(&manager->mutex);
    return available;
}

bool
sc_recording_manager_is_busy(struct sc_recording_manager *manager) {
    enum sc_recording_state state =
        sc_recording_manager_get_state(manager);
    return state == SC_RECORDING_STATE_PREPARING
        || state == SC_RECORDING_STATE_RECORDING
        || state == SC_RECORDING_STATE_FINALIZING
        || state == SC_RECORDING_STATE_AWAITING_SAVE
        || state == SC_RECORDING_STATE_SAVING;
}

enum sc_recording_notification
sc_recording_manager_take_notification(struct sc_recording_manager *manager) {
    sc_mutex_lock(&manager->mutex);
    enum sc_recording_notification notification = manager->notification;
    manager->notification = SC_RECORDING_NOTIFICATION_NONE;
    sc_mutex_unlock(&manager->mutex);
    return notification;
}

void
sc_recording_manager_handle_session_ended(struct sc_recording_manager *manager,
                                          struct sc_recording_session *session,
                                          SDL_Window *window) {
    assert(session->manager == manager);
    sc_recorder_join(&session->recorder);
    char *filename = strdup(session->recorder.filename);
    bool direct = session->direct;
    bool discard = session->discard_on_end;
    bool forced_failure = session->forced_failure;
    bool success = session->success;
    sc_recorder_destroy(&session->recorder);

    sc_mutex_lock(&manager->mutex);
    assert(manager->session == session);
    manager->session = NULL;
    sc_mutex_unlock(&manager->mutex);
    free(session);

    if (discard) {
        if (filename) {
            SDL_RemovePath(filename);
        }
        free(filename);
        sc_mutex_lock(&manager->mutex);
        sc_recording_manager_set_state_locked(
            manager, forced_failure ? SC_RECORDING_STATE_ERROR
                                    : SC_RECORDING_STATE_IDLE,
            forced_failure ? SC_RECORDING_NOTIFICATION_FAILED
                           : SC_RECORDING_NOTIFICATION_NONE);
        sc_mutex_unlock(&manager->mutex);
        sc_recording_manager_post_state(manager);
        return;
    }

    if (!success || !filename) {
        sc_mutex_lock(&manager->mutex);
        sc_recording_manager_set_state_locked(
            manager, SC_RECORDING_STATE_ERROR,
            SC_RECORDING_NOTIFICATION_FAILED);
        sc_mutex_unlock(&manager->mutex);
        sc_recording_manager_post_state(manager);
        if (direct) {
            sc_push_event(SC_EVENT_RECORDER_ERROR);
        }
        free(filename);
        return;
    }

    if (direct) {
        LOGI("Recording saved to %s", filename);
        free(filename);
        sc_mutex_lock(&manager->mutex);
        sc_recording_manager_set_state_locked(
            manager, SC_RECORDING_STATE_IDLE,
            SC_RECORDING_NOTIFICATION_SAVED);
        sc_mutex_unlock(&manager->mutex);
        sc_recording_manager_post_state(manager);
        return;
    }

    sc_recording_save_begin(&manager->save, filename, window);
}

void
sc_recording_manager_destroy(struct sc_recording_manager *manager) {
    if (!manager->initialized) {
        return;
    }
    sc_recording_save_destroy(&manager->save);
    if (manager->session) {
        sc_recorder_stop(&manager->session->recorder);
        sc_recorder_join(&manager->session->recorder);
        sc_recorder_destroy(&manager->session->recorder);
        free(manager->session);
    }
    avcodec_parameters_free(&manager->video_params);
    avcodec_parameters_free(&manager->audio_params);
    av_packet_free(&manager->video_config);
    av_packet_free(&manager->audio_config);
    av_packet_free(&manager->pending_video_keyframe);
    free(manager->pending_filename);
    sc_mutex_destroy(&manager->mutex);
    manager->initialized = false;
}
