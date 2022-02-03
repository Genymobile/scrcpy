#ifndef SC_FILE_PUSHER_H
#define SC_FILE_PUSHER_H

#include "common.h"

#include <stdbool.h>

#include "util/cbuf.h"
#include "util/thread.h"
#include "util/intr.h"

enum sc_file_pusher_action {
    SC_FILE_PUSHER_ACTION_INSTALL_APK,
    SC_FILE_PUSHER_ACTION_PUSH_FILE,
};

struct sc_file_pusher_request {
    enum sc_file_pusher_action action;
    char *file;
};

struct sc_file_pusher_request_queue CBUF(struct sc_file_pusher_request, 16);

struct sc_file_pusher {
    char *serial;
    const char *push_target;
    sc_thread thread;
    sc_mutex mutex;
    sc_cond event_cond;
    bool stopped;
    bool initialized;
    struct sc_file_pusher_request_queue queue;

    struct sc_intr intr;
};

bool
sc_file_pusher_init(struct sc_file_pusher *fp, const char *serial,
                    const char *push_target);

void
sc_file_pusher_destroy(struct sc_file_pusher *fp);

bool
sc_file_pusher_start(struct sc_file_pusher *fp);

void
sc_file_pusher_stop(struct sc_file_pusher *fp);

void
sc_file_pusher_join(struct sc_file_pusher *fp);

// take ownership of file, and will free() it
bool
sc_file_pusher_request(struct sc_file_pusher *fp,
                       enum sc_file_pusher_action action, char *file);

#endif
