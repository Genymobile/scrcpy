#include "file_handler.h"

#include <string.h>
#include <SDL2/SDL_assert.h>
#include "config.h"
#include "command.h"
#include "device.h"
#include "lock_util.h"
#include "log.h"

struct request {
    file_handler_action_t action;
    const char *file;
};

static struct request *
request_new(file_handler_action_t action, const char *file) {
    struct request *req = SDL_malloc(sizeof(*req));
    if (!req) {
        return NULL;
    }
    req->action = action;
    req->file = file;
    return req;
}

static void
request_free(struct request *req) {
    if (!req) {
        return;
    }
    SDL_free((void *) req->file);
    SDL_free((void *) req);
}

static bool
request_queue_is_empty(const struct request_queue *queue) {
    return queue->head == queue->tail;
}

static bool
request_queue_is_full(const struct request_queue *queue) {
    return (queue->head + 1) % REQUEST_QUEUE_SIZE == queue->tail;
}

static bool
request_queue_init(struct request_queue *queue) {
    queue->head = 0;
    queue->tail = 0;
    return true;
}

static void
request_queue_destroy(struct request_queue *queue) {
    int i = queue->tail;
    while (i != queue->head) {
        request_free(queue->reqs[i]);
        i = (i + 1) % REQUEST_QUEUE_SIZE;
    }
}

static bool
request_queue_push(struct request_queue *queue, struct request *req) {
    if (request_queue_is_full(queue)) {
        return false;
    }
    queue->reqs[queue->head] = req;
    queue->head = (queue->head + 1) % REQUEST_QUEUE_SIZE;
    return true;
}

static bool
request_queue_take(struct request_queue *queue, struct request **req) {
    if (request_queue_is_empty(queue)) {
        return false;
    }
    // transfer ownership
    *req = queue->reqs[queue->tail];
    queue->tail = (queue->tail + 1) % REQUEST_QUEUE_SIZE;
    return true;
}

bool
file_handler_init(struct file_handler *file_handler, const char *serial) {

    if (!request_queue_init(&file_handler->queue)) {
        return false;
    }

    if (!(file_handler->mutex = SDL_CreateMutex())) {
        return false;
    }

    if (!(file_handler->event_cond = SDL_CreateCond())) {
        SDL_DestroyMutex(file_handler->mutex);
        return false;
    }

    if (serial) {
        file_handler->serial = SDL_strdup(serial);
        if (!file_handler->serial) {
            LOGW("Cannot strdup serial");
            SDL_DestroyMutex(file_handler->mutex);
            return false;
        }
    } else {
        file_handler->serial = NULL;
    }

    // lazy initialization
    file_handler->initialized = false;

    file_handler->stopped = false;
    file_handler->current_process = PROCESS_NONE;

    return true;
}

void
file_handler_destroy(struct file_handler *file_handler) {
    SDL_DestroyCond(file_handler->event_cond);
    SDL_DestroyMutex(file_handler->mutex);
    request_queue_destroy(&file_handler->queue);
    SDL_free((void *) file_handler->serial);
}

static process_t
install_apk(const char *serial, const char *file) {
    return adb_install(serial, file);
}

static process_t
push_file(const char *serial, const char *file) {
    return adb_push(serial, file, DEVICE_SDCARD_PATH);
}

bool
file_handler_request(struct file_handler *file_handler,
                     file_handler_action_t action,
                     const char *file) {
    bool res;

    // start file_handler if it's used for the first time
    if (!file_handler->initialized) {
        if (!file_handler_start(file_handler)) {
            return false;
        }
        file_handler->initialized = true;
    }

    LOGI("Request to %s %s", action == ACTION_INSTALL_APK ? "install" : "push",
                             file);
    struct request *req = request_new(action, file);
    if (!req) {
        LOGE("Could not create request");
        return false;
    }

    mutex_lock(file_handler->mutex);
    bool was_empty = request_queue_is_empty(&file_handler->queue);
    res = request_queue_push(&file_handler->queue, req);
    if (was_empty) {
        cond_signal(file_handler->event_cond);
    }
    mutex_unlock(file_handler->mutex);
    return res;
}

static int
run_file_handler(void *data) {
    struct file_handler *file_handler = data;

    for (;;) {
        mutex_lock(file_handler->mutex);
        file_handler->current_process = PROCESS_NONE;
        while (!file_handler->stopped
                && request_queue_is_empty(&file_handler->queue)) {
            cond_wait(file_handler->event_cond, file_handler->mutex);
        }
        if (file_handler->stopped) {
            // stop immediately, do not process further events
            mutex_unlock(file_handler->mutex);
            break;
        }
        struct request *req;
        bool non_empty = request_queue_take(&file_handler->queue, &req);
        SDL_assert(non_empty);

        process_t process;
        if (req->action == ACTION_INSTALL_APK) {
            LOGI("Installing %s...", req->file);
            process = install_apk(file_handler->serial, req->file);
        } else {
            LOGI("Pushing %s...", req->file);
            process = push_file(file_handler->serial, req->file);
        }
        file_handler->current_process = process;
        mutex_unlock(file_handler->mutex);

        if (req->action == ACTION_INSTALL_APK) {
            if (process_check_success(process, "adb install")) {
                LOGI("%s successfully installed", req->file);
            } else {
                LOGE("Failed to install %s", req->file);
            }
        } else {
            if (process_check_success(process, "adb push")) {
                LOGI("%s successfully pushed to /sdcard/", req->file);
            } else {
                LOGE("Failed to push %s to /sdcard/", req->file);
            }
        }

        request_free(req);
    }
    return 0;
}

bool
file_handler_start(struct file_handler *file_handler) {
    LOGD("Starting file_handler thread");

    file_handler->thread = SDL_CreateThread(run_file_handler, "file_handler",
                                            file_handler);
    if (!file_handler->thread) {
        LOGC("Could not start file_handler thread");
        return false;
    }

    return true;
}

void
file_handler_stop(struct file_handler *file_handler) {
    mutex_lock(file_handler->mutex);
    file_handler->stopped = true;
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

void
file_handler_join(struct file_handler *file_handler) {
    SDL_WaitThread(file_handler->thread, NULL);
}
