#ifndef SC_DISCONNECT
#define SC_DISCONNECT

#include "common.h"

#include "SDL3/SDL_surface.h"
#include "util/tick.h"
#include "util/thread.h"

// Tool to handle loading the icon and signal timeout when the device is
// unexpectedly disconnected
struct sc_disconnect {
    sc_tick deadline;

    struct sc_thread thread;
    struct sc_mutex mutex;
    struct sc_cond cond;
    bool interrupted;

    const struct sc_disconnect_callbacks *cbs;
    void *cbs_userdata;
};

struct sc_disconnect_callbacks {
    // Called when the disconnected icon is loaded
    void (*on_icon_loaded)(struct sc_disconnect *d, SDL_Surface *icon,
                           void *userdata);

    // Called when the timeout expired (the scrcpy window must be closed)
    void (*on_timeout)(struct sc_disconnect *d, void *userdata);
};

bool
sc_disconnect_start(struct sc_disconnect *d, sc_tick deadline,
                    const struct sc_disconnect_callbacks *cbs,
                    void *cbs_userdata);

void
sc_disconnect_interrupt(struct sc_disconnect *d);

void
sc_disconnect_join(struct sc_disconnect *d);

void
sc_disconnect_destroy(struct sc_disconnect *d);

#endif
