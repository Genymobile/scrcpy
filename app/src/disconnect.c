#include "disconnect.h"

#include <assert.h>

#include "icon.h"
#include "util/log.h"

static int
run(void *userdata) {
    struct sc_disconnect *d = userdata;

    SDL_Surface *icon = sc_icon_load(SC_ICON_FILENAME_DISCONNECTED);
    if (icon) {
        d->cbs->on_icon_loaded(d, icon, d->cbs_userdata);
    } else {
        LOGE("Could not load disconnected icon");
    }

    sc_mutex_lock(&d->mutex);
    bool timed_out = false;
    while (!d->interrupted && !timed_out) {
        timed_out = !sc_cond_timedwait(&d->cond, &d->mutex, d->deadline);
    }
    bool interrupted = d->interrupted;
    sc_mutex_unlock(&d->mutex);

    if (!interrupted) {
        d->cbs->on_timeout(d, d->cbs_userdata);
    }

    return 0;
}

bool
sc_disconnect_start(struct sc_disconnect *d, sc_tick deadline,
                    const struct sc_disconnect_callbacks *cbs,
                    void *cbs_userdata) {
    bool ok = sc_mutex_init(&d->mutex);
    if (!ok) {
        return false;
    }

    ok = sc_cond_init(&d->cond);
    if (!ok) {
        goto error_destroy_mutex;
    }

    d->deadline = deadline;
    d->interrupted = false;

    assert(cbs && cbs->on_icon_loaded && cbs->on_timeout);
    d->cbs = cbs;
    d->cbs_userdata = cbs_userdata;

    ok = sc_thread_create(&d->thread, run, "scrcpy-dis", d);
    if (!ok) {
        goto error_destroy_cond;
    }

    return true;

error_destroy_cond:
    sc_cond_destroy(&d->cond);
error_destroy_mutex:
    sc_mutex_destroy(&d->mutex);

    return false;
}

void
sc_disconnect_interrupt(struct sc_disconnect *d) {
    sc_mutex_lock(&d->mutex);
    d->interrupted = true;
    sc_mutex_unlock(&d->mutex);
    // wake up blocking wait
    sc_cond_signal(&d->cond);
}

void
sc_disconnect_join(struct sc_disconnect *d) {
    sc_thread_join(&d->thread, NULL);
}

void
sc_disconnect_destroy(struct sc_disconnect *d) {
    sc_cond_destroy(&d->cond);
    sc_mutex_destroy(&d->mutex);
}
