#include "common.h"

#include <assert.h>
#include <string.h>

#include "util/audiobuf.h"

static void test_audiobuf_simple(void) {
    struct sc_audiobuf buf;
    uint32_t data[20];

    bool ok = sc_audiobuf_init(&buf, 4, 20);
    assert(ok);

    uint32_t samples[] = {1, 2, 3, 4, 5};
    uint32_t w = sc_audiobuf_write(&buf, samples, 5);
    assert(w == 5);

    uint32_t r = sc_audiobuf_read(&buf, data, 4);
    assert(r == 4);
    assert(!memcmp(data, samples, 16));

    uint32_t samples2[] = {6, 7, 8};
    w = sc_audiobuf_write(&buf, samples2, 3);
    assert(w == 3);

    uint32_t single = 9;
    w = sc_audiobuf_write(&buf, &single, 1);
    assert(w == 1);

    r = sc_audiobuf_read(&buf, &data[4], 8);
    assert(r == 5);

    uint32_t expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    assert(!memcmp(data, expected, 36));

    sc_audiobuf_destroy(&buf);
}

static void test_audiobuf_boundaries(void) {
    struct sc_audiobuf buf;
    uint32_t data[20];

    bool ok = sc_audiobuf_init(&buf, 4, 20);
    assert(ok);

    uint32_t samples[] = {1, 2, 3, 4, 5, 6};
    uint32_t w = sc_audiobuf_write(&buf, samples, 6);
    assert(w == 6);

    w = sc_audiobuf_write(&buf, samples, 6);
    assert(w == 6);

    w = sc_audiobuf_write(&buf, samples, 6);
    assert(w == 6);

    uint32_t r = sc_audiobuf_read(&buf, data, 9);
    assert(r == 9);

    uint32_t expected[] = {1, 2, 3, 4, 5, 6, 1, 2, 3};
    assert(!memcmp(data, expected, 36));

    uint32_t samples2[] = {7, 8, 9, 10, 11};
    w = sc_audiobuf_write(&buf, samples2, 5);
    assert(w == 5);

    uint32_t single = 12;
    w = sc_audiobuf_write(&buf, &single, 1);
    assert(w == 1);

    w = sc_audiobuf_read(&buf, NULL, 3);
    assert(w == 3);

    assert(sc_audiobuf_can_read(&buf) == 12);

    r = sc_audiobuf_read(&buf, data, 12);
    assert(r == 12);

    uint32_t expected2[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    assert(!memcmp(data, expected2, 48));

    sc_audiobuf_destroy(&buf);
}

static void test_audiobuf_partial_read_write(void) {
    struct sc_audiobuf buf;
    uint32_t data[15];

    bool ok = sc_audiobuf_init(&buf, 4, 10);
    assert(ok);

    uint32_t samples[] = {1, 2, 3, 4, 5, 6};
    uint32_t w = sc_audiobuf_write(&buf, samples, 6);
    assert(w == 6);

    w = sc_audiobuf_write(&buf, samples, 6);
    assert(w == 4);

    w = sc_audiobuf_write(&buf, samples, 6);
    assert(w == 0);

    uint32_t r = sc_audiobuf_read(&buf, data, 3);
    assert(r == 3);

    uint32_t expected[] = {1, 2, 3};
    assert(!memcmp(data, expected, 12));

    w = sc_audiobuf_write(&buf, samples, 6);
    assert(w == 3);

    r = sc_audiobuf_read(&buf, data, 15);
    assert(r == 10);
    uint32_t expected2[] = {4, 5, 6, 1, 2, 3, 4, 1, 2, 3};
    assert(!memcmp(data, expected2, 12));

    sc_audiobuf_destroy(&buf);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_audiobuf_simple();
    test_audiobuf_boundaries();
    test_audiobuf_partial_read_write();

    return 0;
}
