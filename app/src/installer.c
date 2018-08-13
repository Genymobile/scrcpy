#include "installer.h"

#include <string.h>
#include "command.h"
#include "lockutil.h"
#include "log.h"

// NOTE(adopi) this can be more generic:
// it could be used with a command queue instead of a filename queue
// then we would have a generic invoker (useful if we want to handle more async commands)

SDL_bool apk_queue_is_empty(const struct apk_queue *queue) {
    return queue->head == queue->tail;
}

SDL_bool apk_queue_is_full(const struct apk_queue *queue) {
    return (queue->head + 1) % APK_QUEUE_SIZE == queue->tail;
}

SDL_bool apk_queue_init(struct apk_queue *queue) {
    queue->head = 0;
    queue->tail = 0;
    return SDL_TRUE;
}

void apk_queue_destroy(struct apk_queue *queue) {
    int i = queue->tail;
    while (i != queue->head) {
        SDL_free(queue->data[i]);
        i = (i + 1) % APK_QUEUE_SIZE;
    }
}

SDL_bool apk_queue_push(struct apk_queue *queue, const char *apk) {
    if (apk_queue_is_full(queue)) {
        return SDL_FALSE;
    }
    queue->data[queue->head] = SDL_strdup(apk);
    queue->head = (queue->head + 1) % APK_QUEUE_SIZE;
    return SDL_TRUE;
}

SDL_bool apk_queue_take(struct apk_queue *queue, char **apk) {
    if (apk_queue_is_empty(queue)) {
        return SDL_FALSE;
    }
    // transfer ownership
    *apk = queue->data[queue->tail];
    queue->tail = (queue->tail + 1) % APK_QUEUE_SIZE;
    return SDL_TRUE;
}

SDL_bool installer_init(struct installer *installer, const char *serial) {

    if (!apk_queue_init(&installer->queue)) {
        return SDL_FALSE;
    }

    if (!(installer->mutex = SDL_CreateMutex())) {
        return SDL_FALSE;
    }

    if (!(installer->event_cond = SDL_CreateCond())) {
        SDL_DestroyMutex(installer->mutex);
        return SDL_FALSE;
    }

    if (serial) {
        installer->serial = SDL_strdup(serial);
        if (!installer->serial) {
            LOGW("Cannot strdup serial");
            SDL_DestroyMutex(installer->mutex);
            return SDL_FALSE;
        }
    } else {
        installer->serial = NULL;
    }

    // lazy initialization
    installer->initialized = SDL_FALSE;

    installer->stopped = SDL_FALSE;
    installer->current_process = PROCESS_NONE;

    return SDL_TRUE;
}

void installer_destroy(struct installer *installer) {
    SDL_DestroyCond(installer->event_cond);
    SDL_DestroyMutex(installer->mutex);
    apk_queue_destroy(&installer->queue);
    SDL_free((void *) installer->serial);
}

SDL_bool installer_install_apk(struct installer *installer, const char *apk) {
    SDL_bool res;

    // start installer if it's used for the first time
    if (!installer->initialized) {
        if (!installer_start(installer)) {
            return SDL_FALSE;
        }
        installer->initialized = SDL_TRUE;
    }

    mutex_lock(installer->mutex);
    SDL_bool was_empty = apk_queue_is_empty(&installer->queue);
    res = apk_queue_push(&installer->queue, apk);
    if (was_empty) {
        cond_signal(installer->event_cond);
    }
    mutex_unlock(installer->mutex);
    return res;
}

static int run_installer(void *data) {
    struct installer *installer = data;

    for (;;) {
        mutex_lock(installer->mutex);
        while (!installer->stopped && apk_queue_is_empty(&installer->queue)) {
            cond_wait(installer->event_cond, installer->mutex);
        }
        if (installer->stopped) {
            // stop immediately, do not process further events
            mutex_unlock(installer->mutex);
            break;
        }
        char *current_apk;
#ifdef BUILD_DEBUG
        bool non_empty = apk_queue_take(&installer->queue, &current_apk);
        SDL_assert(non_empty);
#else
        apk_queue_take(&installer->queue, &current_apk);
#endif

        LOGI("Installing %s...", current_apk);
        process_t process = adb_install(installer->serial, current_apk);
        installer->current_process = process;

        mutex_unlock(installer->mutex);

        if (process_check_success(process, "adb install")) {
            LOGI("%s installed successfully", current_apk);
        } else {
            LOGE("Failed to install %s", current_apk);
        }
        SDL_free(current_apk);
    }
    return 0;
}

SDL_bool installer_start(struct installer *installer) {
    LOGD("Starting installer thread");

    installer->thread = SDL_CreateThread(run_installer, "installer", installer);
    if (!installer->thread) {
        LOGC("Could not start installer thread");
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

void installer_stop(struct installer *installer) {
    mutex_lock(installer->mutex);
    installer->stopped = SDL_TRUE;
    cond_signal(installer->event_cond);
    if (installer->current_process != PROCESS_NONE) {
        if (!cmd_terminate(installer->current_process)) {
            LOGW("Cannot terminate install process");
        }
        cmd_simple_wait(installer->current_process, NULL);
        installer->current_process = PROCESS_NONE;
    }
    mutex_unlock(installer->mutex);
}

void installer_join(struct installer *installer) {
    SDL_WaitThread(installer->thread, NULL);
}
