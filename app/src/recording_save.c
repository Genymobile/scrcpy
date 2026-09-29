#include "recording_save.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>

#include "events.h"
#include "util/log.h"

#define SC_RECORDING_DIRECTORY_FILENAME "recording-last-directory.txt"
#define SC_RECORDING_DIRECTORY_MAX_SIZE 32768

struct sc_recording_dialog_result {
    struct sc_recording_save *save;
    char *path;
    bool error;
};

struct sc_recording_save_job {
    struct sc_recording_save *save;
    char *src;
    char *dst;
    bool success;
};

static char *
sc_recording_get_directory_settings_path(void) {
    char *pref = SDL_GetPrefPath("Genymobile", "scrcpy");
    if (!pref) {
        return NULL;
    }
    char *path = NULL;
    if (asprintf(&path, "%s%s", pref,
                 SC_RECORDING_DIRECTORY_FILENAME) == -1) {
        path = NULL;
    }
    SDL_free(pref);
    return path;
}

static bool
sc_recording_directory_exists(const char *path) {
    SDL_PathInfo info;
    return path && SDL_GetPathInfo(path, &info)
        && info.type == SDL_PATHTYPE_DIRECTORY;
}

static char *
sc_recording_load_last_directory(void) {
    char *settings_path = sc_recording_get_directory_settings_path();
    if (!settings_path) {
        return NULL;
    }
    size_t size = 0;
    void *data = SDL_LoadFile(settings_path, &size);
    free(settings_path);
    if (!data || !size || size > SC_RECORDING_DIRECTORY_MAX_SIZE) {
        SDL_free(data);
        return NULL;
    }

    char *directory = malloc(size + 1);
    if (!directory) {
        SDL_free(data);
        LOG_OOM();
        return NULL;
    }
    memcpy(directory, data, size);
    SDL_free(data);
    while (size && (directory[size - 1] == '\n'
                 || directory[size - 1] == '\r'
                 || directory[size - 1] == '\0')) {
        --size;
    }
    directory[size] = '\0';
    if (!size || !sc_recording_directory_exists(directory)) {
        free(directory);
        return NULL;
    }
    return directory;
}

static char *
sc_recording_get_parent_directory(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *separator = slash;
    if (!separator || (backslash && backslash > separator)) {
        separator = backslash;
    }
    if (!separator) {
        return NULL;
    }
    size_t length = separator - path + 1;
    char *directory = malloc(length + 1);
    if (!directory) {
        LOG_OOM();
        return NULL;
    }
    memcpy(directory, path, length);
    directory[length] = '\0';
    return directory;
}

static void
sc_recording_save_remember_directory(struct sc_recording_save *save,
                                     const char *selected_path) {
    char *directory = sc_recording_get_parent_directory(selected_path);
    if (!directory) {
        return;
    }
    free(save->last_save_directory);
    save->last_save_directory = directory;
}

static void
sc_recording_save_persist_directory(struct sc_recording_save *save) {
    if (!save->last_save_directory) {
        return;
    }
    char *settings_path = sc_recording_get_directory_settings_path();
    if (!settings_path) {
        LOGW("Could not resolve recording directory preferences path");
        return;
    }
    size_t size = strlen(save->last_save_directory);
    if (!SDL_SaveFile(settings_path, save->last_save_directory, size)) {
        LOGW("Could not remember recording directory: %s", SDL_GetError());
    }
    free(settings_path);
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

static void
sc_recording_save_notify(struct sc_recording_save *save,
                         enum sc_recording_save_status status) {
    if (save->callbacks && save->callbacks->on_status_changed) {
        save->callbacks->on_status_changed(save, status,
                                           save->callbacks_userdata);
    }
}

static void SDLCALL
sc_recording_save_dialog_callback(void *userdata,
                                  const char *const *filelist, int filter) {
    (void) filter;
    struct sc_recording_save *save = userdata;
    struct sc_recording_dialog_result *result = calloc(1, sizeof(*result));
    if (!result) {
        LOG_OOM();
        return;
    }
    result->save = save;
    if (!filelist) {
        result->error = true;
    } else if (filelist[0]) {
        result->path = strdup(filelist[0]);
        if (!result->path) {
            result->error = true;
        }
    }
    sc_push_event_with_data(SC_EVENT_RECORDING_SAVE_DIALOG, result);
}

static void
sc_recording_save_show_dialog(struct sc_recording_save *save) {
    free(save->default_save_path);
    const char *directory = save->last_save_directory;
    if (!sc_recording_directory_exists(directory)) {
        directory = SDL_GetUserFolder(SDL_FOLDER_VIDEOS);
        if (!directory) {
            directory = SDL_GetUserFolder(SDL_FOLDER_HOME);
        }
    }
    save->default_save_path = directory
                            ? sc_recording_build_name(directory) : NULL;
    static const SDL_DialogFileFilter filters[] = {
        {"MP4 video", "mp4"},
    };
    SDL_ShowSaveFileDialog(sc_recording_save_dialog_callback, save,
                           save->dialog_window, filters, ARRAY_LEN(filters),
                           save->default_save_path);
}

static bool
sc_recording_save_confirm_discard(struct sc_recording_save *save,
                                  const char *message) {
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Return"},
        {0, 1, "Discard"},
    };
    const SDL_MessageBoxData data = {
        .flags = SDL_MESSAGEBOX_WARNING,
        .window = save->dialog_window,
        .title = "Discard recording?",
        .message = message,
        .numbuttons = ARRAY_LEN(buttons),
        .buttons = buttons,
    };
    int selected = 0;
    return SDL_ShowMessageBox(&data, &selected) && selected == 1;
}

static void
sc_recording_save_discard_pending(struct sc_recording_save *save) {
    if (save->pending_temp_path
            && !SDL_RemovePath(save->pending_temp_path)) {
        LOGW("Could not remove temporary recording %s: %s",
             save->pending_temp_path, SDL_GetError());
    }
    free(save->pending_temp_path);
    save->pending_temp_path = NULL;
    sc_recording_save_notify(save, SC_RECORDING_SAVE_STATUS_DISCARDED);
}

static int
sc_recording_save_worker(void *userdata) {
    struct sc_recording_save_job *job = userdata;
    bool ok = SDL_RenamePath(job->src, job->dst);
    if (!ok) {
        ok = SDL_CopyFile(job->src, job->dst);
        if (ok && !SDL_RemovePath(job->src)) {
            LOGW("Saved recording, but could not remove temp file %s: %s",
                 job->src, SDL_GetError());
        }
    }
    job->success = ok;
    sc_push_event_with_data(SC_EVENT_RECORDING_SAVE_COMPLETE, job);
    return 0;
}

void
sc_recording_save_init(struct sc_recording_save *save,
                       const struct sc_recording_save_callbacks *callbacks,
                       void *callbacks_userdata) {
    memset(save, 0, sizeof(*save));
    save->last_save_directory = sc_recording_load_last_directory();
    save->callbacks = callbacks;
    save->callbacks_userdata = callbacks_userdata;
}

void
sc_recording_save_begin(struct sc_recording_save *save, char *temp_path,
                        SDL_Window *window) {
    assert(!save->pending_temp_path);
    save->pending_temp_path = temp_path;
    save->dialog_window = window;
    sc_recording_save_notify(save, SC_RECORDING_SAVE_STATUS_AWAITING);
    sc_recording_save_show_dialog(save);
}

void
sc_recording_save_handle_dialog(void *userdata) {
    struct sc_recording_dialog_result *result = userdata;
    struct sc_recording_save *save = result->save;
    free(save->default_save_path);
    save->default_save_path = NULL;

    if (result->error) {
        sc_recording_save_notify(save, SC_RECORDING_SAVE_STATUS_FAILED);
        if (!sc_recording_save_confirm_discard(
                save, "The save dialog failed. Return to try again, or "
                      "discard the temporary recording.")) {
            sc_recording_save_notify(save,
                                     SC_RECORDING_SAVE_STATUS_AWAITING);
            sc_recording_save_show_dialog(save);
        } else {
            sc_recording_save_discard_pending(save);
        }
    } else if (!result->path) {
        if (sc_recording_save_confirm_discard(
                save, "The recording has not been saved. Return to the "
                      "save dialog, or discard it permanently.")) {
            sc_recording_save_discard_pending(save);
        } else {
            sc_recording_save_show_dialog(save);
        }
    } else {
        sc_recording_save_remember_directory(save, result->path);
        sc_recording_save_persist_directory(save);
        struct sc_recording_save_job *job = calloc(1, sizeof(*job));
        if (!job) {
            LOG_OOM();
            goto save_error;
        }
        job->save = save;
        job->src = strdup(save->pending_temp_path);
        job->dst = result->path;
        result->path = NULL;
        if (!job->src) {
            free(job->dst);
            free(job);
            goto save_error;
        }
        save->job = job;
        sc_recording_save_notify(save, SC_RECORDING_SAVE_STATUS_SAVING);
        if (!sc_thread_create(&save->thread, sc_recording_save_worker,
                              "scrcpy-save", job)) {
            save->job = NULL;
            free(job->src);
            free(job->dst);
            free(job);
            goto save_error;
        }
        save->thread_started = true;
    }
    free(result->path);
    free(result);
    return;

save_error:
    sc_recording_save_notify(save, SC_RECORDING_SAVE_STATUS_FAILED);
    free(result->path);
    free(result);
}

void
sc_recording_save_handle_complete(void *userdata) {
    struct sc_recording_save_job *job = userdata;
    struct sc_recording_save *save = job->save;
    assert(job == save->job);
    if (save->thread_started) {
        sc_thread_join(&save->thread, NULL);
        save->thread_started = false;
    }
    save->job = NULL;
    if (job->success) {
        LOGI("Recording saved to %s", job->dst);
        free(save->pending_temp_path);
        save->pending_temp_path = NULL;
        sc_recording_save_notify(save, SC_RECORDING_SAVE_STATUS_SAVED);
    } else {
        LOGE("Could not save recording to %s: %s", job->dst,
             SDL_GetError());
        sc_recording_save_notify(save, SC_RECORDING_SAVE_STATUS_FAILED);
        if (!sc_recording_save_confirm_discard(
                save, "Saving failed. Return to choose another location, "
                      "or discard the temporary recording.")) {
            sc_recording_save_notify(save,
                                     SC_RECORDING_SAVE_STATUS_AWAITING);
            sc_recording_save_show_dialog(save);
        } else {
            sc_recording_save_discard_pending(save);
        }
    }
    free(job->src);
    free(job->dst);
    free(job);
}

void
sc_recording_save_destroy(struct sc_recording_save *save) {
    if (save->thread_started) {
        sc_thread_join(&save->thread, NULL);
    }
    free(save->pending_temp_path);
    free(save->last_save_directory);
    free(save->default_save_path);
}
