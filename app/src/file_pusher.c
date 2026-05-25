#include "file_pusher.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adb/adb.h"
#include "control_msg.h"
#include "controller.h"
#include "util/log.h"

#define DEFAULT_PUSH_TARGET "/sdcard/Download/"

static const char *
basename_of(const char *path) {
    const char *sep = strrchr(path, '/');
#ifdef _WIN32
    const char *bsl = strrchr(path, '\\');
    if (bsl && (!sep || bsl > sep)) {
        sep = bsl;
    }
#endif
    return sep ? sep + 1 : path;
}

static char *
build_remote_path(const char *target, const char *local_file) {
    const char *base = basename_of(local_file);
    size_t target_len = strlen(target);
    bool needs_sep = target_len == 0 || target[target_len - 1] != '/';
    size_t total = target_len + (needs_sep ? 1 : 0) + strlen(base) + 1;
    char *result = malloc(total);
    if (!result) {
        LOG_OOM();
        return NULL;
    }
    snprintf(result, total, "%s%s%s", target, needs_sep ? "/" : "", base);
    return result;
}

static void
sc_file_pusher_request_destroy(struct sc_file_pusher_request *req) {
    free(req->file);
}

bool
sc_file_pusher_init(struct sc_file_pusher *fp, const char *serial,
                    const char *push_target,
                    struct sc_controller *controller) {
    assert(serial);
    assert(controller);

    sc_vecdeque_init(&fp->queue);

    bool ok = sc_mutex_init(&fp->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&fp->event_cond);
    if (!ok) {
        sc_mutex_destroy(&fp->mutex);
        return false;
    }

    ok = sc_intr_init(&fp->intr);
    if (!ok) {
        sc_cond_destroy(&fp->event_cond);
        sc_mutex_destroy(&fp->mutex);
        return false;
    }

    fp->serial = strdup(serial);
    if (!fp->serial) {
        LOG_OOM();
        sc_intr_destroy(&fp->intr);
        sc_cond_destroy(&fp->event_cond);
        sc_mutex_destroy(&fp->mutex);
        return false;
    }

    // lazy initialization
    fp->initialized = false;

    fp->stopped = false;

    fp->push_target = push_target ? push_target : DEFAULT_PUSH_TARGET;
    fp->controller = controller;

    return true;
}

void
sc_file_pusher_destroy(struct sc_file_pusher *fp) {
    sc_cond_destroy(&fp->event_cond);
    sc_mutex_destroy(&fp->mutex);
    sc_intr_destroy(&fp->intr);
    free(fp->serial);

    while (!sc_vecdeque_is_empty(&fp->queue)) {
        struct sc_file_pusher_request *req = sc_vecdeque_popref(&fp->queue);
        assert(req);
        sc_file_pusher_request_destroy(req);
    }
}

bool
sc_file_pusher_request(struct sc_file_pusher *fp,
                       enum sc_file_pusher_action action, char *file) {
    // start file_pusher if it's used for the first time
    if (!fp->initialized) {
        if (!sc_file_pusher_start(fp)) {
            return false;
        }
        fp->initialized = true;
    }

    LOGI("Request to %s %s", action == SC_FILE_PUSHER_ACTION_INSTALL_APK
                                 ? "install" : "push",
                             file);
    struct sc_file_pusher_request req = {
        .action = action,
        .file = file,
    };

    sc_mutex_lock(&fp->mutex);
    bool was_empty = sc_vecdeque_is_empty(&fp->queue);
    bool res = sc_vecdeque_push(&fp->queue, req);
    if (!res) {
        LOG_OOM();
        sc_mutex_unlock(&fp->mutex);
        return false;
    }

    if (was_empty) {
        sc_cond_signal(&fp->event_cond);
    }
    sc_mutex_unlock(&fp->mutex);

    return true;
}

static int
run_file_pusher(void *data) {
    struct sc_file_pusher *fp = data;
    struct sc_intr *intr = &fp->intr;

    const char *serial = fp->serial;
    assert(serial);

    for (;;) {
        sc_mutex_lock(&fp->mutex);
        while (!fp->stopped && sc_vecdeque_is_empty(&fp->queue)) {
            sc_cond_wait(&fp->event_cond, &fp->mutex);
        }
        if (fp->stopped) {
            // stop immediately, do not process further events
            sc_mutex_unlock(&fp->mutex);
            break;
        }

        assert(!sc_vecdeque_is_empty(&fp->queue));
        struct sc_file_pusher_request req = sc_vecdeque_pop(&fp->queue);
        sc_mutex_unlock(&fp->mutex);

        if (req.action == SC_FILE_PUSHER_ACTION_INSTALL_APK) {
            LOGI("Installing %s...", req.file);
            bool ok = sc_adb_install(intr, serial, req.file, 0);
            if (ok) {
                LOGI("%s successfully installed", req.file);
            } else {
                LOGE("Failed to install %s", req.file);
            }
        } else {
            LOGI("Pushing %s...", req.file);
            bool ok = sc_adb_push(intr, serial, req.file, fp->push_target, 0);
            if (ok) {
                LOGI("%s successfully pushed to %s", req.file, fp->push_target);
                char *remote = build_remote_path(fp->push_target, req.file);
                if (remote) {
                    struct sc_control_msg msg;
                    msg.type = SC_CONTROL_MSG_TYPE_SCAN_FILE;
                    msg.scan_file.path = remote; // ownership transferred
                    if (!sc_controller_push_msg(fp->controller, &msg)) {
                        LOGW("Could not request MediaStore scan for %s",
                             remote);
                        free(remote);
                    }
                }
            } else {
                LOGE("Failed to push %s to %s", req.file, fp->push_target);
            }
        }

        sc_file_pusher_request_destroy(&req);
    }
    return 0;
}

bool
sc_file_pusher_start(struct sc_file_pusher *fp) {
    LOGD("Starting file_pusher thread");

    bool ok = sc_thread_create(&fp->thread, run_file_pusher, "scrcpy-file", fp);
    if (!ok) {
        LOGE("Could not start file_pusher thread");
        return false;
    }

    return true;
}

void
sc_file_pusher_stop(struct sc_file_pusher *fp) {
    if (fp->initialized) {
        sc_mutex_lock(&fp->mutex);
        fp->stopped = true;
        sc_cond_signal(&fp->event_cond);
        sc_intr_interrupt(&fp->intr);
        sc_mutex_unlock(&fp->mutex);
    }
}

void
sc_file_pusher_join(struct sc_file_pusher *fp) {
    if (fp->initialized) {
        sc_thread_join(&fp->thread, NULL);
    }
}
