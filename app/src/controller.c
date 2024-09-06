#include "controller.h"

#include <assert.h>

#include "util/log.h"

// Drop droppable events above this limit
#define SC_CONTROL_MSG_QUEUE_LIMIT 60

static void
sc_controller_receiver_on_ended(struct sc_receiver *receiver, bool error,
                                void *userdata) {
    (void) receiver;

    struct sc_controller *controller = userdata;
    // Forward the event to the controller listener
    controller->cbs->on_ended(controller, error, controller->cbs_userdata);
}

bool
sc_controller_init(struct sc_controller *controller, sc_socket control_socket,
                   const struct sc_controller_callbacks *cbs,
                   void *cbs_userdata) {
    sc_vecdeque_init(&controller->queue);

    // Add 4 to support 4 non-droppable events without re-allocation
    bool ok = sc_vecdeque_reserve(&controller->queue,
                                  SC_CONTROL_MSG_QUEUE_LIMIT + 4);
    if (!ok) {
        return false;
    }

    static const struct sc_receiver_callbacks receiver_cbs = {
        .on_ended = sc_controller_receiver_on_ended,
    };

    ok = sc_receiver_init(&controller->receiver, control_socket, &receiver_cbs,
                          controller);
    if (!ok) {
        sc_vecdeque_destroy(&controller->queue);
        return false;
    }

    ok = sc_mutex_init(&controller->mutex);
    if (!ok) {
        sc_receiver_destroy(&controller->receiver);
        sc_vecdeque_destroy(&controller->queue);
        return false;
    }

    ok = sc_cond_init(&controller->msg_cond);
    if (!ok) {
        sc_receiver_destroy(&controller->receiver);
        sc_mutex_destroy(&controller->mutex);
        sc_vecdeque_destroy(&controller->queue);
        return false;
    }

    controller->control_socket = control_socket;
    controller->stopped = false;

    assert(cbs && cbs->on_ended);
    controller->cbs = cbs;
    controller->cbs_userdata = cbs_userdata;

    return true;
}

void
sc_controller_configure(struct sc_controller *controller,
                        struct sc_acksync *acksync,
                        struct sc_uhid_devices *uhid_devices) {
    controller->receiver.acksync = acksync;
    controller->receiver.uhid_devices = uhid_devices;
}

void
sc_controller_destroy(struct sc_controller *controller) {
    sc_cond_destroy(&controller->msg_cond);
    sc_mutex_destroy(&controller->mutex);

    while (!sc_vecdeque_is_empty(&controller->queue)) {
        struct sc_control_msg *msg = sc_vecdeque_popref(&controller->queue);
        assert(msg);
        sc_control_msg_destroy(msg);
    }
    sc_vecdeque_destroy(&controller->queue);

    sc_receiver_destroy(&controller->receiver);
}

bool
sc_controller_push_msg(struct sc_controller *controller,
                       const struct sc_control_msg *msg) {
    if (sc_get_log_level() <= SC_LOG_LEVEL_VERBOSE) {
        sc_control_msg_log(msg);
    }

    bool pushed = false;

    sc_mutex_lock(&controller->mutex);
    size_t size = sc_vecdeque_size(&controller->queue);
    if (size < SC_CONTROL_MSG_QUEUE_LIMIT) {
        bool was_empty = sc_vecdeque_is_empty(&controller->queue);
        sc_vecdeque_push_noresize(&controller->queue, *msg);
        pushed = true;
        if (was_empty) {
            sc_cond_signal(&controller->msg_cond);
        }
    } else if (!sc_control_msg_is_droppable(msg)) {
        bool ok = sc_vecdeque_push(&controller->queue, *msg);
        if (ok) {
            pushed = true;
        } else {
            // A non-droppable event must be dropped anyway
            LOG_OOM();
        }
    }
    // Otherwise, the msg is discarded

    sc_mutex_unlock(&controller->mutex);

    return pushed;
}

static bool
process_msg(struct sc_controller *controller,
            const struct sc_control_msg *msg, bool *eos) {
    static uint8_t serialized_msg[SC_CONTROL_MSG_MAX_SIZE];
    size_t length = sc_control_msg_serialize(msg, serialized_msg);
    if (!length) {
        *eos = false;
        return false;
    }

    ssize_t w =
        net_send_all(controller->control_socket, serialized_msg, length);
    if ((size_t) w != length) {
        *eos = true;
        return false;
    }

    return true;
}

static int
run_controller(void *data) {
    struct sc_controller *controller = data;

    bool error = false;

    for (;;) {
        sc_mutex_lock(&controller->mutex);
        while (!controller->stopped
                && sc_vecdeque_is_empty(&controller->queue)) {
            sc_cond_wait(&controller->msg_cond, &controller->mutex);
        }
        if (controller->stopped) {
            // stop immediately, do not process further msgs
            sc_mutex_unlock(&controller->mutex);
            LOGD("Controller stopped");
            break;
        }

        assert(!sc_vecdeque_is_empty(&controller->queue));
        struct sc_control_msg msg = sc_vecdeque_pop(&controller->queue);
        sc_mutex_unlock(&controller->mutex);

        bool eos;
        bool ok = process_msg(controller, &msg, &eos);
        sc_control_msg_destroy(&msg);
        if (!ok) {
            if (eos) {
                LOGD("Controller stopped (socket closed)");
            } // else error already logged
            error = !eos;
            break;
        }
    }

    controller->cbs->on_ended(controller, error, controller->cbs_userdata);

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

    if (!sc_receiver_start(&controller->receiver)) {
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
    sc_receiver_join(&controller->receiver);
}
