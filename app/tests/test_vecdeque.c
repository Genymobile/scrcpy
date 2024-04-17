#include "common.h"

#include <assert.h>

#include "util/vecdeque.h"

#define pr(pv) \
({ \
    fprintf(stderr, "cap=%lu origin=%lu size=%lu\n", (pv)->cap, (pv)->origin, (pv)->size); \
    for (size_t i = 0; i < (pv)->cap; ++i) \
        fprintf(stderr, "%d ", (pv)->data[i]); \
    fprintf(stderr, "\n"); \
})

static void test_vecdeque_push_pop(void) {
    struct SC_VECDEQUE(int) vdq = SC_VECDEQUE_INITIALIZER;

    assert(sc_vecdeque_is_empty(&vdq));
    assert(sc_vecdeque_size(&vdq) == 0);

    bool ok = sc_vecdeque_push(&vdq, 5);
    assert(ok);
    assert(sc_vecdeque_size(&vdq) == 1);

    ok = sc_vecdeque_push(&vdq, 12);
    assert(ok);
    assert(sc_vecdeque_size(&vdq) == 2);

    int v = sc_vecdeque_pop(&vdq);
    assert(v == 5);
    assert(sc_vecdeque_size(&vdq) == 1);

    ok = sc_vecdeque_push(&vdq, 7);
    assert(ok);
    assert(sc_vecdeque_size(&vdq) == 2);

    int *p = sc_vecdeque_popref(&vdq);
    assert(p);
    assert(*p == 12);
    assert(sc_vecdeque_size(&vdq) == 1);

    v = sc_vecdeque_pop(&vdq);
    assert(v == 7);
    assert(sc_vecdeque_size(&vdq) == 0);
    assert(sc_vecdeque_is_empty(&vdq));

    sc_vecdeque_destroy(&vdq);
}

static void test_vecdeque_reserve(void) {
    struct SC_VECDEQUE(int) vdq = SC_VECDEQUE_INITIALIZER;

    bool ok = sc_vecdeque_reserve(&vdq, 20);
    assert(ok);
    assert(vdq.cap == 20);

    assert(sc_vecdeque_size(&vdq) == 0);

    for (size_t i = 0; i < 20; ++i) {
        ok = sc_vecdeque_push(&vdq, i);
        assert(ok);
    }

    assert(sc_vecdeque_size(&vdq) == 20);

    // It is now full

    for (int i = 0; i < 5; ++i) {
        int v = sc_vecdeque_pop(&vdq);
        assert(v == i);
    }
    assert(sc_vecdeque_size(&vdq) == 15);

    for (int i = 20; i < 25; ++i) {
        ok = sc_vecdeque_push(&vdq, i);
        assert(ok);
    }

    assert(sc_vecdeque_size(&vdq) == 20);
    assert(vdq.cap == 20);

    // Now, the content wraps around the ring buffer:
    // 20 21 22 23 24  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
    //                 ^
    //                 origin

    // It is now full, let's reserve some space
    ok = sc_vecdeque_reserve(&vdq, 30);
    assert(ok);
    assert(vdq.cap == 30);

    assert(sc_vecdeque_size(&vdq) == 20);

    for (int i = 0; i < 20; ++i) {
        // We should retrieve the items we inserted in order
        int v = sc_vecdeque_pop(&vdq);
        assert(v == i + 5);
    }

    assert(sc_vecdeque_size(&vdq) == 0);

    sc_vecdeque_destroy(&vdq);
}

static void test_vecdeque_grow(void) {
    struct SC_VECDEQUE(int) vdq = SC_VECDEQUE_INITIALIZER;

    bool ok = sc_vecdeque_reserve(&vdq, 20);
    assert(ok);
    assert(vdq.cap == 20);

    assert(sc_vecdeque_size(&vdq) == 0);

    for (int i = 0; i < 500; ++i) {
        ok = sc_vecdeque_push(&vdq, i);
        assert(ok);
    }

    assert(sc_vecdeque_size(&vdq) == 500);

    for (int i = 0; i < 100; ++i) {
        int v = sc_vecdeque_pop(&vdq);
        assert(v == i);
    }

    assert(sc_vecdeque_size(&vdq) == 400);

    for (int i = 500; i < 1000; ++i) {
        ok = sc_vecdeque_push(&vdq, i);
        assert(ok);
    }

    assert(sc_vecdeque_size(&vdq) == 900);

    for (int i = 100; i < 1000; ++i) {
        int v = sc_vecdeque_pop(&vdq);
        assert(v == i);
    }

    assert(sc_vecdeque_size(&vdq) == 0);

    sc_vecdeque_destroy(&vdq);
}

static void test_vecdeque_push_hole(void) {
    struct SC_VECDEQUE(int) vdq = SC_VECDEQUE_INITIALIZER;

    bool ok = sc_vecdeque_reserve(&vdq, 20);
    assert(ok);
    assert(vdq.cap == 20);

    assert(sc_vecdeque_size(&vdq) == 0);

    for (int i = 0; i < 20; ++i) {
        int *p = sc_vecdeque_push_hole(&vdq);
        assert(p);
        *p = i * 10;
    }

    assert(sc_vecdeque_size(&vdq) == 20);

    for (int i = 0; i < 10; ++i) {
        int v = sc_vecdeque_pop(&vdq);
        assert(v == i * 10);
    }

    assert(sc_vecdeque_size(&vdq) == 10);

    for (int i = 20; i < 30; ++i) {
        int *p = sc_vecdeque_push_hole(&vdq);
        assert(p);
        *p = i * 10;
    }

    assert(sc_vecdeque_size(&vdq) == 20);

    for (int i = 10; i < 30; ++i) {
        int v = sc_vecdeque_pop(&vdq);
        assert(v == i * 10);
    }

    assert(sc_vecdeque_size(&vdq) == 0);

    sc_vecdeque_destroy(&vdq);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_vecdeque_push_pop();
    test_vecdeque_reserve();
    test_vecdeque_grow();
    test_vecdeque_push_hole();

    return 0;
}
