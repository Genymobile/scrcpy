#include <assert.h>

#include "util/queue.h"

struct foo {
    int value;
    struct foo *next;
};

static void test_queue(void) {
    struct my_queue QUEUE(struct foo) queue;
    queue_init(&queue);

    assert(queue_is_empty(&queue));

    struct foo v1 = { .value = 42 };
    struct foo v2 = { .value = 27 };

    queue_push(&queue, next, &v1);
    queue_push(&queue, next, &v2);

    struct foo *foo;

    assert(!queue_is_empty(&queue));
    queue_take(&queue, next, &foo);
    assert(foo->value == 42);

    assert(!queue_is_empty(&queue));
    queue_take(&queue, next, &foo);
    assert(foo->value == 27);

    assert(queue_is_empty(&queue));
}

int main(void) {
    test_queue();
    return 0;
}
