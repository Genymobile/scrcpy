#ifndef SC_RECORDING_SAVE_H
#define SC_RECORDING_SAVE_H

#include "common.h"

#include <stdbool.h>
#include <SDL3/SDL.h>

#include "util/thread.h"

enum sc_recording_save_status {
    SC_RECORDING_SAVE_STATUS_AWAITING,
    SC_RECORDING_SAVE_STATUS_SAVING,
    SC_RECORDING_SAVE_STATUS_SAVED,
    SC_RECORDING_SAVE_STATUS_FAILED,
    SC_RECORDING_SAVE_STATUS_DISCARDED,
};

struct sc_recording_save;
struct sc_recording_save_job;

struct sc_recording_save_callbacks {
    void (*on_status_changed)(struct sc_recording_save *save,
                              enum sc_recording_save_status status,
                              void *userdata);
};

struct sc_recording_save {
    char *pending_temp_path;
    char *last_save_directory;
    char *default_save_path;
    SDL_Window *dialog_window;

    struct sc_recording_save_job *job;
    sc_thread thread;
    bool thread_started;

    const struct sc_recording_save_callbacks *callbacks;
    void *callbacks_userdata;
};

void
sc_recording_save_init(struct sc_recording_save *save,
                       const struct sc_recording_save_callbacks *callbacks,
                       void *callbacks_userdata);

// Take ownership of temp_path and start the save flow.
void
sc_recording_save_begin(struct sc_recording_save *save, char *temp_path,
                        SDL_Window *window);

void
sc_recording_save_handle_dialog(void *userdata);

void
sc_recording_save_handle_complete(void *userdata);

void
sc_recording_save_destroy(struct sc_recording_save *save);

#endif
