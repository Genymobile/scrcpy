#ifndef SC_HID_REPLAY_H
#define SC_HID_REPLAY_H

#include "common.h"

#include <stdbool.h>

#include "aoa_hid.h"
#include "hid_event_serializer.h"
#include "hid_keyboard.h"
#include "hid_mouse.h"
#include "util/thread.h"

struct sc_hidr_thread_and_queue {
    const char *filename;
    sc_thread thread;
    sc_mutex mutex; // guards queue access.
    sc_cond event_cond;
    atomic_bool stopped;
    struct sc_hid_event_ptr_queue queue;
};

struct sc_hidr {
    struct sc_hidr_thread_and_queue taq_replay;
    struct sc_hidr_thread_and_queue taq_record;

    struct sc_aoa *aoa;
    bool enable_keyboard;
    bool enable_mouse;
};

bool
sc_hidr_init(struct sc_hidr *hidr, struct sc_aoa *aoa,
             const char *record_filename, const char *replay_filename,
             bool enable_keyboard, bool enable_mouse);

void
sc_hidr_destroy(struct sc_hidr *hidr);

bool
sc_hidr_start_record(struct sc_hidr *hidr);

bool
sc_hidr_start_replay(struct sc_hidr *hidr);

// Can be called from any thread, after sc_hidr_start_record().
void
sc_hidr_observe_event_for_record(struct sc_hidr *hidr,
                                 const struct sc_hid_event *event);

// Can be called from any thread, after sc_hidr_start_replay().
// Takes ownership of the |event| pointee.
void
sc_hidr_trigger_event_for_replay(struct sc_hidr *hidr,
                                 struct sc_hid_event *event);

void
sc_hidr_stop_record(struct sc_hidr *hidr);

void
sc_hidr_stop_replay(struct sc_hidr *hidr);

#endif
