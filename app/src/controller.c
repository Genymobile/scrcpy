#include "controller.h"

#include <assert.h>

#include "util/log.h"

bool
sc_controller_init(struct sc_controller *controller, sc_socket control_socket,
                   struct sc_acksync *acksync) {
    cbuf_init(&controller->queue);

    bool ok = receiver_init(&controller->receiver, control_socket, acksync);
    if (!ok) {
        return false;
    }

    ok = sc_mutex_init(&controller->mutex);
    if (!ok) {
        receiver_destroy(&controller->receiver);
        return false;
    }

    ok = sc_cond_init(&controller->msg_cond);
    if (!ok) {
        receiver_destroy(&controller->receiver);
        sc_mutex_destroy(&controller->mutex);
        return false;
    }

    controller->control_socket = control_socket;
    controller->stopped = false;

    return true;
}

void
sc_controller_destroy(struct sc_controller *controller) {
    sc_cond_destroy(&controller->msg_cond);
    sc_mutex_destroy(&controller->mutex);

    struct sc_control_msg msg;
    while (cbuf_take(&controller->queue, &msg)) {
        sc_control_msg_destroy(&msg);
    }

    receiver_destroy(&controller->receiver);
}

bool
sc_controller_push_msg(struct sc_controller *controller,
                       const struct sc_control_msg *msg) {
    if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
        sc_control_msg_log(msg);
    }

    sc_mutex_lock(&controller->mutex);
    bool was_empty = cbuf_is_empty(&controller->queue);
    bool res = cbuf_push(&controller->queue, *msg);
    if (was_empty) {
        sc_cond_signal(&controller->msg_cond);
    }
    sc_mutex_unlock(&controller->mutex);
    return res;
}

static bool
process_msg(struct sc_controller *controller,
            const struct sc_control_msg *msg) {
    static unsigned char serialized_msg[SC_CONTROL_MSG_MAX_SIZE];
    size_t length = sc_control_msg_serialize(msg, serialized_msg);
    if (!length) {
        return false;
    }
    ssize_t w =
        net_send_all(controller->control_socket, serialized_msg, length);
    return (size_t) w == length;
}

static int
run_controller(void *data) {
    struct sc_controller *controller = data;

    for (;;) {
        sc_mutex_lock(&controller->mutex);
        while (!controller->stopped && cbuf_is_empty(&controller->queue)) {
            sc_cond_wait(&controller->msg_cond, &controller->mutex);
        }
        if (controller->stopped) {
            // stop immediately, do not process further msgs
            sc_mutex_unlock(&controller->mutex);
            break;
        }
        struct sc_control_msg msg;
        bool non_empty = cbuf_take(&controller->queue, &msg);
        assert(non_empty);
        (void) non_empty;
        sc_mutex_unlock(&controller->mutex);

        bool ok = process_msg(controller, &msg);
        sc_control_msg_destroy(&msg);
        if (!ok) {
            LOGD("Could not write msg to socket");
            break;
        }
    }
    return 0;
}

bool
sc_controller_start(struct sc_controller *controller) {
    LOGD("Starting controller thread");

    bool ok = sc_thread_create(&controller->thread, run_controller,
                               "scrcpy-ctl", controller);
    if (!ok) {
        LOGE("Could not start controller thread");
        return false;
    }

    if (!receiver_start(&controller->receiver)) {
        sc_controller_stop(controller);
        sc_thread_join(&controller->thread, NULL);
        return false;
    }

    return true;
}

void
sc_controller_stop(struct sc_controller *controller) {
    sc_mutex_lock(&controller->mutex);
    controller->stopped = true;
    sc_cond_signal(&controller->msg_cond);
    sc_mutex_unlock(&controller->mutex);
}

void
sc_controller_join(struct sc_controller *controller) {
    sc_thread_join(&controller->thread, NULL);
    receiver_join(&controller->receiver);
}
