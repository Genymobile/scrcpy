#include "common.h"

#include <assert.h>
#include <string.h>

#include "util/cbuf.h"

struct int_queue CBUF(int, 32);

static void test_cbuf_empty(void) {
    struct int_queue queue;
    cbuf_init(&queue);

    assert(cbuf_is_empty(&queue));

    bool push_ok = cbuf_push(&queue, 42);
    assert(push_ok);
    assert(!cbuf_is_empty(&queue));

    int item;
    bool take_ok = cbuf_take(&queue, &item);
    assert(take_ok);
    assert(cbuf_is_empty(&queue));

    bool take_empty_ok = cbuf_take(&queue, &item);
    assert(!take_empty_ok); // the queue is empty
}

static void test_cbuf_full(void) {
    struct int_queue queue;
    cbuf_init(&queue);

    assert(!cbuf_is_full(&queue));

    // fill the queue
    for (int i = 0; i < 32; ++i) {
        bool ok = cbuf_push(&queue, i);
        assert(ok);
    }
    bool ok = cbuf_push(&queue, 42);
    assert(!ok); // the queue if full

    int item;
    bool take_ok = cbuf_take(&queue, &item);
    assert(take_ok);
    assert(!cbuf_is_full(&queue));
}

static void test_cbuf_push_take(void) {
    struct int_queue queue;
    cbuf_init(&queue);

    bool push1_ok = cbuf_push(&queue, 42);
    assert(push1_ok);

    bool push2_ok = cbuf_push(&queue, 35);
    assert(push2_ok);

    int item;

    bool take1_ok = cbuf_take(&queue, &item);
    assert(take1_ok);
    assert(item == 42);

    bool take2_ok = cbuf_take(&queue, &item);
    assert(take2_ok);
    assert(item == 35);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_cbuf_empty();
    test_cbuf_full();
    test_cbuf_push_take();
    return 0;
}
