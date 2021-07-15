#include "controller.h"

#include <assert.h>

#include "util/log.h"

bool
controller_init(struct controller *controller, socket_t control_socket) {
    cbuf_init(&controller->queue);

    bool ok = receiver_init(&controller->receiver, control_socket);
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
controller_destroy(struct controller *controller) {
    sc_cond_destroy(&controller->msg_cond);
    sc_mutex_destroy(&controller->mutex);

    struct control_msg msg;
    while (cbuf_take(&controller->queue, &msg)) {
        control_msg_destroy(&msg);
    }

    receiver_destroy(&controller->receiver);
}

bool
controller_push_msg(struct controller *controller,
                      const struct control_msg *msg) {
    if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
        control_msg_log(msg);
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
process_msg(struct controller *controller,
              const struct control_msg *msg) {
    static unsigned char serialized_msg[CONTROL_MSG_MAX_SIZE];
    size_t length = control_msg_serialize(msg, serialized_msg);
    if (!length) {
        return false;
    }
    ssize_t w = net_send_all(controller->control_socket, serialized_msg, length);
    return (size_t) w == length;
}

static int
run_controller(void *data) {
    struct controller *controller = data;

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
        struct control_msg msg;
        bool non_empty = cbuf_take(&controller->queue, &msg);
        assert(non_empty);
        (void) non_empty;
        sc_mutex_unlock(&controller->mutex);

        bool ok = process_msg(controller, &msg);
        control_msg_destroy(&msg);
        if (!ok) {
            LOGD("Could not write msg to socket");
            break;
        }
    }
    return 0;
}

bool
controller_start(struct controller *controller) {
    LOGD("Starting controller thread");

    bool ok = sc_thread_create(&controller->thread, run_controller,
                               "controller", controller);
    if (!ok) {
        LOGC("Could not start controller thread");
        return false;
    }

    if (!receiver_start(&controller->receiver)) {
        controller_stop(controller);
        sc_thread_join(&controller->thread, NULL);
        return false;
    }

    return true;
}

void
controller_stop(struct controller *controller) {
    sc_mutex_lock(&controller->mutex);
    controller->stopped = true;
    sc_cond_signal(&controller->msg_cond);
    sc_mutex_unlock(&controller->mutex);
}

void
controller_join(struct controller *controller) {
    sc_thread_join(&controller->thread, NULL);
    receiver_join(&controller->receiver);
}
