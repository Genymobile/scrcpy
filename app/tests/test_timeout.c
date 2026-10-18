#include "common.h"

#include <assert.h>

#include "util/timeout.h"

static void on_timeout(struct sc_timeout *timeout, void *userdata) {
    (void) timeout;
    unsigned *count = userdata;
    ++*count;
}

static const struct sc_timeout_callbacks cbs = {
    .on_timeout = on_timeout,
};

static void test_timeout_expired(void) {
    struct sc_timeout timeout;
    unsigned count = 0;

    bool ok = sc_timeout_init(&timeout);
    assert(ok);

    // the deadline is already reached
    ok = sc_timeout_start(&timeout, 0, &cbs, &count);
    assert(ok);

    sc_timeout_join(&timeout);
    assert(count == 1);

    sc_timeout_destroy(&timeout);
}

static void test_timeout_stopped(void) {
    struct sc_timeout timeout;
    unsigned count = 0;

    bool ok = sc_timeout_init(&timeout);
    assert(ok);

    sc_tick deadline = sc_tick_now() + SC_TICK_FROM_SEC(60);
    ok = sc_timeout_start(&timeout, deadline, &cbs, &count);
    assert(ok);

    // stop before the deadline
    sc_timeout_stop(&timeout);
    sc_timeout_join(&timeout);
    assert(count == 0);

    sc_timeout_destroy(&timeout);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_timeout_expired();
    test_timeout_stopped();

    return 0;
}
