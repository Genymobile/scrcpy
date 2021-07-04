#include "common.h"

#include <assert.h>

#include "util/queue.h"

struct foo {
    int value;
    struct foo *next;
};

static void test_queue(void) {
    struct my_queue SC_QUEUE(struct foo) queue;
    sc_queue_init(&queue);

    assert(sc_queue_is_empty(&queue));

    struct foo v1 = { .value = 42 };
    struct foo v2 = { .value = 27 };

    sc_queue_push(&queue, next, &v1);
    sc_queue_push(&queue, next, &v2);

    struct foo *foo;

    assert(!sc_queue_is_empty(&queue));
    sc_queue_take(&queue, next, &foo);
    assert(foo->value == 42);

    assert(!sc_queue_is_empty(&queue));
    sc_queue_take(&queue, next, &foo);
    assert(foo->value == 27);

    assert(sc_queue_is_empty(&queue));
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_queue();
    return 0;
}
