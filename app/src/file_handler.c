#include "file_handler.h"

#include <string.h>
#include "command.h"
#include "device.h"
#include "lockutil.h"
#include "log.h"

void request_free(struct request *req) {
    if (!req) {
        return;
    }
    SDL_free(req->file);
    free(req);
}

SDL_bool request_queue_is_empty(const struct request_queue *queue) {
    return queue->head == queue->tail;
}

SDL_bool request_queue_is_full(const struct request_queue *queue) {
    return (queue->head + 1) % REQUEST_QUEUE_SIZE == queue->tail;
}

SDL_bool request_queue_init(struct request_queue *queue) {
    queue->head = 0;
    queue->tail = 0;
    return SDL_TRUE;
}

void request_queue_destroy(struct request_queue *queue) {
    int i = queue->tail;
    while (i != queue->head) {
        request_free(queue->reqs[i]);
        i = (i + 1) % REQUEST_QUEUE_SIZE;
    }
}

SDL_bool request_queue_push(struct request_queue *queue, struct request *req) {
    if (request_queue_is_full(queue)) {
        return SDL_FALSE;
    }
    queue->reqs[queue->head] = req;
    queue->head = (queue->head + 1) % REQUEST_QUEUE_SIZE;
    return SDL_TRUE;
}

SDL_bool request_queue_take(struct request_queue *queue, struct request **req) {
    if (request_queue_is_empty(queue)) {
        return SDL_FALSE;
    }
    // transfer ownership
    *req = queue->reqs[queue->tail];
    queue->tail = (queue->tail + 1) % REQUEST_QUEUE_SIZE;
    return SDL_TRUE;
}

SDL_bool file_handler_init(struct file_handler *file_handler, const char *serial) {

    if (!request_queue_init(&file_handler->queue)) {
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
    request_queue_destroy(&file_handler->queue);
    SDL_free((void *) file_handler->serial);
}

static process_t install_apk(const char *serial, const char *file) {
    return adb_install(serial, file);
}

static process_t push_file(const char *serial, const char *file) {
    return adb_push(serial, file, DEVICE_SDCARD_PATH);
}

SDL_bool file_handler_add_request(struct file_handler *file_handler, const char *file, req_action_t action) {
    SDL_bool res;
    struct request *req = (struct request *) malloc(sizeof(struct request));

    // start file_handler if it's used for the first time
    if (!file_handler->initialized) {
        if (!file_handler_start(file_handler)) {
            return SDL_FALSE;
        }
        file_handler->initialized = SDL_TRUE;
    }

    LOGI("Adding request %s", file);
    req->file = SDL_strdup(file);
    req->action = action;
    if (action == INSTALL_APK) {
        req->func = install_apk;
    } else {
        req->func = push_file;
    }

    mutex_lock(file_handler->mutex);
    SDL_bool was_empty = request_queue_is_empty(&file_handler->queue);
    res = request_queue_push(&file_handler->queue, req);
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
        while (!file_handler->stopped && request_queue_is_empty(&file_handler->queue)) {
            cond_wait(file_handler->event_cond, file_handler->mutex);
        }
        if (file_handler->stopped) {
            // stop immediately, do not process further events
            mutex_unlock(file_handler->mutex);
            break;
        }
        struct request *req;
        process_t process;
        const char *cmd;
#ifdef BUILD_DEBUG
        bool non_empty = request_queue_take(&file_handler->queue, &req);
        SDL_assert(non_empty);
#else
        request_queue_take(&file_handler->queue, &req);
#endif
        LOGI("Processing %s...", req->file);
        process = req->func(file_handler->serial, req->file);
        file_handler->current_process = process;
        mutex_unlock(file_handler->mutex);

        if (req->action == INSTALL_APK) {
            cmd = "adb install";
        } else {
            cmd = "adb push";
        }

        if (process_check_success(process, cmd)) {
            LOGI("Process %s successfully", req->file);
        } else {
            LOGE("Failed to process %s", req->file);
        }
        request_free(req);
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
