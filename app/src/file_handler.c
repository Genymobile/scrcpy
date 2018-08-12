#include "file_handler.h"

#include <string.h>
#include "command.h"
#include "lockutil.h"
#include "log.h"

// NOTE(adopi) this can be more generic:
// it could be used with a command queue instead of a filename queue
// then we would have a generic invoker (useful if we want to handle more async commands)

SDL_bool file_queue_is_empty(const struct file_queue *queue) {
    return queue->head == queue->tail;
}

SDL_bool file_queue_is_full(const struct file_queue *queue) {
    return (queue->head + 1) % FILE_QUEUE_SIZE == queue->tail;
}

SDL_bool file_queue_init(struct file_queue *queue) {
    queue->head = 0;
    queue->tail = 0;
    return SDL_TRUE;
}

void file_queue_destroy(struct file_queue *queue) {
    int i = queue->tail;
    while (i != queue->head) {
        SDL_free(queue->data[i]);
        i = (i + 1) % FILE_QUEUE_SIZE;
    }
}

SDL_bool file_queue_push(struct file_queue *queue, const char *file) {
    if (file_queue_is_full(queue)) {
        return SDL_FALSE;
    }
    queue->data[queue->head] = SDL_strdup(file);
    queue->head = (queue->head + 1) % FILE_QUEUE_SIZE;
    return SDL_TRUE;
}

SDL_bool file_queue_take(struct file_queue *queue, char **file) {
    if (file_queue_is_empty(queue)) {
        return SDL_FALSE;
    }
    // transfer ownership
    *file = queue->data[queue->tail];
    queue->tail = (queue->tail + 1) % FILE_QUEUE_SIZE;
    return SDL_TRUE;
}

SDL_bool file_handler_init(struct file_handler *file_handler, const char *serial) {

    if (!file_queue_init(&file_handler->queue)) {
        return SDL_FALSE;
    }

    if (!(file_handler->mutex = SDL_CreateMutex())) {
        return SDL_FALSE;
    }

    if (!(file_handler->event_cond = SDL_CreateCond())) {
        SDL_DestroyMutex(file_handler->mutex);
        return SDL_FALSE;
    }

    if (serial) {
        file_handler->serial = SDL_strdup(serial);
        if (!file_handler->serial) {
            LOGW("Cannot strdup serial");
            SDL_DestroyMutex(file_handler->mutex);
            return SDL_FALSE;
        }
    } else {
        file_handler->serial = NULL;
    }

    // lazy initialization
    file_handler->initialized = SDL_FALSE;

    file_handler->stopped = SDL_FALSE;
    file_handler->current_process = PROCESS_NONE;

    return SDL_TRUE;
}

void file_handler_destroy(struct file_handler *file_handler) {
    SDL_DestroyCond(file_handler->event_cond);
    SDL_DestroyMutex(file_handler->mutex);
    file_queue_destroy(&file_handler->queue);
    SDL_free((void *) file_handler->serial);
}

SDL_bool file_handler_do(struct file_handler *file_handler, const char *file) {
    SDL_bool res;

    // start file_handler if it's used for the first time
    if (!file_handler->initialized) {
        if (!file_handler_start(file_handler)) {
            return SDL_FALSE;
        }
        file_handler->initialized = SDL_TRUE;
    }

    mutex_lock(file_handler->mutex);
    SDL_bool was_empty = file_queue_is_empty(&file_handler->queue);
    res = file_queue_push(&file_handler->queue, file);
    if (was_empty) {
        cond_signal(file_handler->event_cond);
    }
    mutex_unlock(file_handler->mutex);
    return res;
}

static int run_file_handler(void *data) {
    struct file_handler *file_handler = data;

    for (;;) {
        mutex_lock(file_handler->mutex);
        file_handler->current_process = PROCESS_NONE;
        while (!file_handler->stopped && file_queue_is_empty(&file_handler->queue)) {
            cond_wait(file_handler->event_cond, file_handler->mutex);
        }
        if (file_handler->stopped) {
            // stop immediately, do not process further events
            mutex_unlock(file_handler->mutex);
            break;
        }
        char *current_apk;
#ifdef BUILD_DEBUG
        bool non_empty = file_queue_take(&file_handler->queue, &current_apk);
        SDL_assert(non_empty);
#else
        file_queue_take(&file_handler->queue, &current_apk);
#endif
        LOGI("Installing %s...", current_apk);
        process_t process = adb_install(file_handler->serial, current_apk);
        file_handler->current_process = process;

        mutex_unlock(file_handler->mutex);

        if (process_check_success(process, "adb install")) {
            LOGI("%s installed successfully", current_apk);
        } else {
            LOGE("Failed to install %s", current_apk);
        }
        SDL_free(current_apk);
    }
    return 0;
}

SDL_bool file_handler_start(struct file_handler *file_handler) {
    LOGD("Starting file_handler thread");

    file_handler->thread = SDL_CreateThread(run_file_handler, "file_handler", file_handler);
    if (!file_handler->thread) {
        LOGC("Could not start file_handler thread");
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void file_handler_stop(struct file_handler *file_handler) {
    mutex_lock(file_handler->mutex);
    file_handler->stopped = SDL_TRUE;
    cond_signal(file_handler->event_cond);
    if (file_handler->current_process != PROCESS_NONE) {
        if (!cmd_terminate(file_handler->current_process)) {
            LOGW("Cannot terminate install process");
        }
        cmd_simple_wait(file_handler->current_process, NULL);
        file_handler->current_process = PROCESS_NONE;
    }
    mutex_unlock(file_handler->mutex);
}

void file_handler_join(struct file_handler *file_handler) {
    SDL_WaitThread(file_handler->thread, NULL);
}
