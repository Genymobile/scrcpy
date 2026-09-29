#ifndef SC_RECORDING_MANAGER_H
#define SC_RECORDING_MANAGER_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

#include "options.h"
#include "recording_save.h"
#include "trait/packet_sink.h"
#include "util/tick.h"

enum sc_recording_state {
    SC_RECORDING_STATE_IDLE,
    SC_RECORDING_STATE_PREPARING,
    SC_RECORDING_STATE_RECORDING,
    SC_RECORDING_STATE_FINALIZING,
    SC_RECORDING_STATE_AWAITING_SAVE,
    SC_RECORDING_STATE_SAVING,
    SC_RECORDING_STATE_ERROR,
};

enum sc_recording_notification {
    SC_RECORDING_NOTIFICATION_NONE,
    SC_RECORDING_NOTIFICATION_STARTED,
    SC_RECORDING_NOTIFICATION_SAVED,
    SC_RECORDING_NOTIFICATION_FAILED,
};

struct sc_recording_session;

struct sc_recording_manager {
    struct sc_packet_sink video_packet_sink;
    struct sc_packet_sink audio_packet_sink;

    sc_mutex mutex;
    bool initialized;
    bool dynamic_enabled;
    bool control;
    bool video_requested;
    bool audio_requested;
    bool video_ready;
    bool audio_ready;
    bool audio_available;
    bool streams_closed;

    AVCodecContext *video_ctx;
    AVCodecContext *audio_ctx;
    AVPacket *video_config;
    AVPacket *audio_config;
    AVPacket *pending_video_keyframe;

    struct sc_recording_session *session;
    enum sc_recording_state state;
    enum sc_recording_notification notification;
    sc_tick prepare_deadline;
    enum sc_orientation orientation;

    char *pending_filename;
    enum sc_record_format pending_format;
    bool pending_direct;

    struct sc_recording_save save;
};

bool
sc_recording_manager_init(struct sc_recording_manager *manager,
                          bool dynamic_enabled, bool control, bool video,
                          bool audio, enum sc_orientation orientation);

void
sc_recording_manager_destroy(struct sc_recording_manager *manager);

bool
sc_recording_manager_start_initial(struct sc_recording_manager *manager,
                                   const char *filename,
                                   enum sc_record_format format);

bool
sc_recording_manager_start(struct sc_recording_manager *manager);

bool
sc_recording_manager_stop(struct sc_recording_manager *manager);

enum sc_recording_state
sc_recording_manager_get_state(struct sc_recording_manager *manager);

bool
sc_recording_manager_is_available(struct sc_recording_manager *manager);

bool
sc_recording_manager_is_busy(struct sc_recording_manager *manager);

enum sc_recording_notification
sc_recording_manager_take_notification(struct sc_recording_manager *manager);

void
sc_recording_manager_handle_session_ended(struct sc_recording_manager *manager,
                                          struct sc_recording_session *session,
                                          SDL_Window *window);

#endif
