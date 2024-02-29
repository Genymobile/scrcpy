#include "util/log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <SDL2/SDL.h> // for file I/O helpers.

#include "hid_event_serializer.h"
#include "hid_replay.h"
#include "util/tick.h"

// There are three threads of interest:
// - The thread where the HID event is emitted (aoa).
// - The thread where the replay is run (run_hid_replay). This may sleep
//   occasionally as part of the event replay. When there are no events to
//   replay, it will wait for the io_thread to provide new data.
// - The I/O thread where the data is read, to feed the replay thread.
struct sc_hidr_replay_thread_state {
    struct sc_hidr *hidr;
    struct sc_hid_event_parser hep;
    sc_mutex io_mutex; // guards access to hep, io_thread_stopped and io_cond.
    sc_cond io_cond;
    sc_thread io_thread;
    bool io_thread_stopped;
};

static bool
sc_hidr_thread_and_queue_init(struct sc_hidr_thread_and_queue *taq,
                              const char *filename) {
    taq->filename = filename;
    if (!filename) {
        return true;
    }
    sc_vecdeque_init(&taq->queue);

    bool ok = sc_mutex_init(&taq->mutex);
    if (!ok) {
        sc_vecdeque_destroy(&taq->queue);
        return false;
    }

    ok = sc_cond_init(&taq->event_cond);
    if (!ok) {
        sc_mutex_destroy(&taq->mutex);
        sc_vecdeque_destroy(&taq->queue);
        return false;
    }
    taq->stopped = false;
    return true;
}

static void
sc_hidr_thread_and_queue_destroy(struct sc_hidr_thread_and_queue *taq) {
    if (!taq->filename) {
        return;
    }
    // Sanity check: once started, sc_hidr_thread_and_queue_destroy must only
    // be called after sc_hidr_thread_and_queue_stop has returned. That implies
    // that taq->thread has terminated, and that there is no concurrent access
    // to the mutex/queue any more.
    assert(taq->stopped);
    sc_cond_destroy(&taq->event_cond);
    sc_mutex_destroy(&taq->mutex);
    while (!sc_vecdeque_is_empty(&taq->queue)) {
        struct sc_hid_event *event = sc_vecdeque_pop(&taq->queue);
        assert(event);
        sc_hid_event_destroy(event);
        free(event);
    }
    sc_vecdeque_destroy(&taq->queue);
}

static void
sc_hidr_thread_and_queue_stop(struct sc_hidr_thread_and_queue *taq) {
    assert(taq->filename); // mutex etc only initialized when filename is set.
    taq->stopped = true;
    sc_mutex_lock(&taq->mutex);
    sc_cond_signal(&taq->event_cond);
    sc_mutex_unlock(&taq->mutex);
    sc_thread_join(&taq->thread, NULL);
}

static bool
sc_hidr_is_accepted_hid_event(struct sc_hidr *hidr,
                              const struct sc_hid_event *event) {
    // 1 is HID_KEYBOARD_ACCESSORY_ID from hid_keyboard.c
    if (event->accessory_id == 1) {
        return hidr->enable_keyboard;
    }
    // 2 is HID_MOUSE_ACCESSORY_ID from hid_mouse.c
    if (event->accessory_id == 2) {
        return hidr->enable_mouse;
    }
    LOGD("Unrecognized accessory_id: %" PRIu16, event->accessory_id);
    return false;
}

bool
sc_hidr_init(struct sc_hidr *hidr, struct sc_aoa *aoa,
             const char *record_filename, const char *replay_filename,
             bool enable_keyboard, bool enable_mouse) {
    if (record_filename && replay_filename &&
            !strcmp(record_filename, replay_filename)) {
        // TODO: Add more comprehensive check that accounts for equivalent
        // file paths, symlinks, etc. Like C++'s std::filesystem::equivalent.
        LOGE("--hid-record and --hid-replay are set to the same file!");
        LOGE("Exiting early to avoid an infinite feedback loop.");
        return false;
    }
    if (!sc_hidr_thread_and_queue_init(&hidr->taq_replay, replay_filename)) {
        return false;
    }
    if (!sc_hidr_thread_and_queue_init(&hidr->taq_record, record_filename)) {
        sc_hidr_thread_and_queue_destroy(&hidr->taq_replay);
        return false;
    }
    hidr->aoa = aoa;
    hidr->enable_mouse = enable_mouse;
    hidr->enable_keyboard = enable_keyboard;
    return true;
}

void
sc_hidr_destroy(struct sc_hidr *hidr) {
    sc_hidr_thread_and_queue_destroy(&hidr->taq_replay);
    sc_hidr_thread_and_queue_destroy(&hidr->taq_record);
}

static void
run_hid_record_to_file(struct sc_hidr *hidr) {
    struct sc_hidr_thread_and_queue *taq = &hidr->taq_record;
    SDL_RWops *io = SDL_RWFromFile(taq->filename, "wb");
    if (!io) {
        LOGE("Unable to open file for HID recording: %s", taq->filename);
        return;
    }

    struct sc_hid_event_serializer hes;
    sc_hid_event_serializer_init(&hes);
    for (;;) {
        sc_mutex_lock(&taq->mutex);
        while (!taq->stopped && sc_vecdeque_is_empty(&taq->queue)) {
            sc_cond_wait(&taq->event_cond, &taq->mutex);
        }
        if (taq->stopped) {
            sc_mutex_unlock(&taq->mutex);
            break;
        }

        bool ok = true;
        assert(!sc_vecdeque_is_empty(&taq->queue));
        while (!sc_vecdeque_is_empty(&taq->queue) && ok) {
            struct sc_hid_event *event = sc_vecdeque_pop(&taq->queue);
            ok = sc_hid_event_serializer_update(&hes, event);
            sc_hid_event_destroy(event);
            free(event); // balances sc_hidr_observe_event_for_record.
        }
        sc_mutex_unlock(&taq->mutex);

        if (!ok) {
            LOGE("Failed to serialize for HID recording to %s", taq->filename);
            break;
        }

        assert(hes.data_len); // Non-zero because at least one event was seen.
        size_t written_size = SDL_RWwrite(io, hes.data, 1, hes.data_len);
        if (written_size != hes.data_len) {
            LOGE("Failed to write line for HID recording to %s: %s"
                    " (expected to write %zu bytes, but written %zu instead)",
                    taq->filename, SDL_GetError(), hes.data_len, written_size);
            break;
        }
        sc_hid_event_serializer_mark_as_read(&hes);
    }
    sc_hid_event_serializer_destroy(&hes);

    SDL_RWclose(io);
    LOGI("Finished HID recording to: %s", taq->filename);
}

static int
run_hid_replay_read_input(void *rts_data) {
    struct sc_hidr_replay_thread_state *rts = rts_data;
    struct sc_hidr *hidr = rts->hidr;
    struct sc_hidr_thread_and_queue *taq = &hidr->taq_replay;
    const char *filename = taq->filename;
    struct sc_hid_event_parser *hep = &rts->hep;

    SDL_RWops *io = SDL_RWFromFile(filename, "rb");
    if (!io) {
        LOGE("Unable to read HID replay from %s: %s", filename, SDL_GetError());
        sc_mutex_lock(&rts->io_mutex);
        rts->io_thread_stopped = true;
        sc_cond_signal(&rts->io_cond);
        sc_mutex_unlock(&rts->io_mutex);
        return 0;
    }
    // When the size can be determined upfront, assume that we can read all
    // data at once. Do so, so we can replay without worry about slow disks
    // resulting in events being replayed too late.
    bool want_all_at_once = SDL_RWsize(io) != -1;
    if (want_all_at_once) {
        LOGD("Starting to read all data for HID replay from %s", filename);
        size_t size;
        char *data = SDL_LoadFile_RW(io, &size, 1); // = reads & closes file.
        if (!data) {
            LOGE("Unable to read HID replay from file: %s", filename);
        } else {
            LOGD("Read %zu bytes from %s", size, filename);
            sc_mutex_lock(&rts->io_mutex);
            if (!sc_hid_event_parser_append_data(hep, data, size)) {
                LOGE("Failed to initialize HID event parser from %s", filename);
            }
            sc_mutex_unlock(&rts->io_mutex);
            SDL_free(data);
        }
    } else {
        LOGD("Starting to stream data for HID replay from %s", filename);
        size_t data_buffer_size = 1024;
        char data[1024];
        for (;;) {
            size_t size_read = SDL_RWread(io, data, 1, data_buffer_size);
            if (!size_read) {
                LOGD("End of data stream for HID replay from %s", filename);
                break;
            }
            sc_mutex_lock(&rts->io_mutex);
            bool ok = sc_hid_event_parser_append_data(hep, data, size_read);
            if (ok) {
                sc_cond_signal(&rts->io_cond);
            }
            sc_mutex_unlock(&rts->io_mutex);
            if (!ok) {
                LOGE("Failed to copy HID replay data from %s", filename);
                break;
            }
        }
        SDL_RWclose(io);
    }

    sc_mutex_lock(&rts->io_mutex);
    rts->io_thread_stopped = true;
    sc_cond_signal(&rts->io_cond);
    sc_mutex_unlock(&rts->io_mutex);
    return 0;
}

static void
run_hid_replay_from_input(struct sc_hidr *hidr,
                          struct sc_hid_event_parser *hep,
                          bool *had_any_event,
                          sc_tick *last_timestamp_p) {
    assert(sc_hid_event_parser_has_next(hep));

    struct sc_hidr_thread_and_queue *taq = &hidr->taq_replay;
    sc_mutex_lock(&taq->mutex);
    struct sc_hid_event *hid_event;
    while ((hid_event = sc_hid_event_parser_get_next(hep)) != NULL) {
        if (taq->stopped) {
            sc_hid_event_destroy(hid_event);
            free(hid_event);
            break;
        }
        if (!sc_hidr_is_accepted_hid_event(hidr, hid_event)) {
            sc_hid_event_destroy(hid_event);
            free(hid_event);
            continue;
        }
        sc_tick ms_to_sleep =
            *had_any_event ? hid_event->timestamp - *last_timestamp_p : 0;
        if (ms_to_sleep < 0) {
            LOGD("HID replay tried to back in time with timestamp: %" PRItick,
                    hid_event->timestamp);
            ms_to_sleep = 0;
        }

        if (ms_to_sleep) {
            sc_tick deadline = sc_tick_now() + SC_TICK_FROM_MS(ms_to_sleep);
            bool ok = true;
            while (!taq->stopped && ok) {
                ok = sc_cond_timedwait(&taq->event_cond, &taq->mutex, deadline);
            }
        }

        if (taq->stopped) {
            sc_hid_event_destroy(hid_event);
            free(hid_event);
            break;
        }

        *had_any_event = true;
        *last_timestamp_p = hid_event->timestamp;

        sc_mutex_unlock(&taq->mutex);
        sc_hidr_trigger_event_for_replay(hidr, hid_event);
        sc_mutex_lock(&taq->mutex);
    }
    sc_mutex_unlock(&taq->mutex);
}

static int
run_hid_record(void *data) {
    struct sc_hidr *hidr = data;
    run_hid_record_to_file(hidr);
    return 0;
}

static int
run_hid_replay(void *data) {
    struct sc_hidr *hidr = data;

    struct sc_hidr_replay_thread_state rts;
    rts.hidr = hidr;
    if (!sc_mutex_init(&rts.io_mutex)) {
        LOGE("Failed to initialize mutex for HID replay");
        return 0;
    }
    if (!sc_cond_init(&rts.io_cond)) {
        LOGE("Failed to initialize cond for HID replay");
        sc_mutex_destroy(&rts.io_mutex);
        return 0;
    }
    sc_hid_event_parser_init(&rts.hep, hidr->taq_replay.filename);
    rts.io_thread_stopped = false;

    // Start thread to read input.
    if (!sc_thread_create(&rts.io_thread, run_hid_replay_read_input,
                "scrcpyHIDinp", &rts)) {
        LOGE("Failed to start thread to read input for HID replay");
        sc_hid_event_parser_destroy(&rts.hep);
        sc_cond_destroy(&rts.io_cond);
        sc_mutex_destroy(&rts.io_mutex);
        return 0;
    }

    // Receive data from input thread and forward events to aoa.
    LOGD("Waiting for input to commence HID replay.");
    bool had_any_event = false;
    sc_tick last_timestamp = 0;
    sc_mutex_lock(&rts.io_mutex);
    while (!hidr->taq_replay.stopped &&
            !sc_hid_event_parser_has_error(&rts.hep)) {
        if (!sc_hid_event_parser_has_next(&rts.hep)) {
            if (!sc_hid_event_parser_has_unparsed_data(&rts.hep)) {
                if (rts.io_thread_stopped) {
                    break;
                }
                sc_cond_wait(&rts.io_cond, &rts.io_mutex);
            }
            sc_hid_event_parser_parse_all_data(&rts.hep);
            continue;
        }
        // Unlock IO mutex because we're going to potentially be blocked by the
        // hidr.taq_replay->mutex, and don't want that to block the IO thread.
        sc_mutex_unlock(&rts.io_mutex);
        run_hid_replay_from_input(hidr, &rts.hep, &had_any_event,
                &last_timestamp);
        sc_mutex_lock(&rts.io_mutex);
    }
    sc_mutex_unlock(&rts.io_mutex);

    // Print diagnostic information.
    LOGD("End of input for HID replay from %s", hidr->taq_replay.filename);
    if (sc_hid_event_parser_has_error(&rts.hep)) {
        LOGE("Invalid HID replay data in %s", hidr->taq_replay.filename);
    } else if (sc_hid_event_parser_has_unparsed_data(&rts.hep) ||
            sc_hid_event_parser_has_next(&rts.hep)) {
        LOGE("Did not finish replay of %s", hidr->taq_replay.filename);
    } else if (!had_any_event) {
        LOGE("Did not find any replay data in %s", hidr->taq_replay.filename);
    } else {
        LOGD("Successfully replayed all data in %s", hidr->taq_replay.filename);
    }

    // Clean up when everything is done.
    sc_thread_join(&rts.io_thread, NULL);
    sc_hid_event_parser_destroy(&rts.hep);
    sc_cond_destroy(&rts.io_cond);
    sc_mutex_destroy(&rts.io_mutex);
    LOGI("Finished HID replay from %s", hidr->taq_replay.filename);
    return 0;
}

bool
sc_hidr_start_record(struct sc_hidr *hidr) {
    // sc_hidr_start_record is called before sc_aoa_start is called (which
    // starts the thread that will access hidr_to_notify). Therefore we can
    // safely modify aoa->hidr_to_notify here.
    hidr->aoa->hidr_to_notify = hidr;
    bool ok = sc_thread_create(&hidr->taq_record.thread, run_hid_record,
                               "scrcpyHIDrecord", hidr);
    if (!ok) {
        LOGE("Could not start HID recorder thread");
        return false;
    }
    LOGI("Recording HID input to: %s", hidr->taq_record.filename);
    return true;
}

bool
sc_hidr_start_replay(struct sc_hidr *hidr) {
    bool ok = sc_thread_create(&hidr->taq_replay.thread, run_hid_replay,
                               "scrcpyHIDreplay", hidr);
    if (!ok) {
        LOGE("Could not start HID replay thread");
        return false;
    }
    LOGI("Replaying HID input from: %s", hidr->taq_replay.filename);
    return true;
}

void
sc_hidr_observe_event_for_record(struct sc_hidr *hidr,
                                 const struct sc_hid_event *event) {
    assert(hidr->taq_record.filename);
    if (!sc_hidr_is_accepted_hid_event(hidr, event)) {
        return;
    }
    // event is not owned, so we need to make a copy first.
    struct sc_hid_event *hid_event = malloc(sizeof(struct sc_hid_event));
    unsigned char *buffer = malloc(event->size);
    if (!buffer || !hid_event) {
        LOG_OOM();
        free(buffer);
        free(hid_event);
        return;
    }
    memcpy(buffer, event->buffer, event->size);
    sc_hid_event_init(hid_event, event->accessory_id, buffer, event->size);
    hid_event->timestamp = SC_TICK_TO_MS(sc_tick_now());

    sc_mutex_lock(&hidr->taq_record.mutex);
    bool ok = false;
    if (!hidr->taq_record.stopped) {
        bool was_empty = sc_vecdeque_is_empty(&hidr->taq_record.queue);
        ok = sc_vecdeque_push(&hidr->taq_record.queue, hid_event);
        if (!ok) {
            LOG_OOM();
        } else if (was_empty) {
            sc_cond_signal(&hidr->taq_record.event_cond);
        }
    }
    sc_mutex_unlock(&hidr->taq_record.mutex);

    if (!ok) {
        sc_hid_event_destroy(hid_event);
        free(hid_event);
    }
}

void
sc_hidr_trigger_event_for_replay(struct sc_hidr *hidr,
                                 struct sc_hid_event *event) {
    assert(hidr->taq_replay.filename);
    // We should already have filtered unwanted events earlier:
    assert(sc_hidr_is_accepted_hid_event(hidr, event));
    if (hidr->taq_replay.stopped) {
        sc_hid_event_destroy(event);
    } else {
        // Note: may indirectly trigger sc_hidr_observe_event_for_record.
        // To avoid deadlocks we avoid unnecessary use of mutexes here and
        // among callers.
        sc_aoa_push_hid_event(hidr->aoa, event);
    }
    // Most callers of sc_aoa_push_hid_event pass a stack-allocated event.
    // |event| here is heap-allocated by sc_hid_event_parser in run_hid_replay.
    free(event); // balances sc_hid_event_parser_parse_all_data.
}

void
sc_hidr_stop_record(struct sc_hidr *hidr) {
    assert(hidr->taq_record.filename);
    sc_hidr_thread_and_queue_stop(&hidr->taq_record);
}

void
sc_hidr_stop_replay(struct sc_hidr *hidr) {
    assert(hidr->taq_replay.filename);
    sc_hidr_thread_and_queue_stop(&hidr->taq_replay);
}
