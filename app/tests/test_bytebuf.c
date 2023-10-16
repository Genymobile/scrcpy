#include "common.h"

#include <assert.h>
#include <string.h>

#include "util/bytebuf.h"

static void test_bytebuf_simple(void) {
    struct sc_bytebuf buf;
    uint8_t data[20];

    bool ok = sc_bytebuf_init(&buf, 20);
    assert(ok);

    sc_bytebuf_write(&buf, (uint8_t *) "hello", sizeof("hello") - 1);
    assert(sc_bytebuf_can_read(&buf) == 5);

    sc_bytebuf_read(&buf, data, 4);
    assert(!strncmp((char *) data, "hell", 4));

    sc_bytebuf_write(&buf, (uint8_t *) " world", sizeof(" world") - 1);
    assert(sc_bytebuf_can_read(&buf) == 7);

    sc_bytebuf_write(&buf, (uint8_t *) "!", 1);
    assert(sc_bytebuf_can_read(&buf) == 8);

    sc_bytebuf_read(&buf, &data[4], 8);
    assert(sc_bytebuf_can_read(&buf) == 0);

    data[12] = '\0';
    assert(!strcmp((char *) data, "hello world!"));
    assert(sc_bytebuf_can_read(&buf) == 0);

    sc_bytebuf_destroy(&buf);
}

static void test_bytebuf_boundaries(void) {
    struct sc_bytebuf buf;
    uint8_t data[20];

    bool ok = sc_bytebuf_init(&buf, 20);
    assert(ok);

    sc_bytebuf_write(&buf, (uint8_t *) "hello ", sizeof("hello ") - 1);
    assert(sc_bytebuf_can_read(&buf) == 6);

    sc_bytebuf_write(&buf, (uint8_t *) "hello ", sizeof("hello ") - 1);
    assert(sc_bytebuf_can_read(&buf) == 12);

    sc_bytebuf_write(&buf, (uint8_t *) "hello ", sizeof("hello ") - 1);
    assert(sc_bytebuf_can_read(&buf) == 18);

    sc_bytebuf_read(&buf, data, 9);
    assert(!strncmp((char *) data, "hello hel", 9));
    assert(sc_bytebuf_can_read(&buf) == 9);

    sc_bytebuf_write(&buf, (uint8_t *) "world", sizeof("world") - 1);
    assert(sc_bytebuf_can_read(&buf) == 14);

    sc_bytebuf_write(&buf, (uint8_t *) "!", 1);
    assert(sc_bytebuf_can_read(&buf) == 15);

    sc_bytebuf_skip(&buf, 3);
    assert(sc_bytebuf_can_read(&buf) == 12);

    sc_bytebuf_read(&buf, data, 12);
    data[12] = '\0';
    assert(!strcmp((char *) data, "hello world!"));
    assert(sc_bytebuf_can_read(&buf) == 0);

    sc_bytebuf_destroy(&buf);
}

static void test_bytebuf_two_steps_write(void) {
    struct sc_bytebuf buf;
    uint8_t data[20];

    bool ok = sc_bytebuf_init(&buf, 20);
    assert(ok);

    sc_bytebuf_write(&buf, (uint8_t *) "hello ", sizeof("hello ") - 1);
    assert(sc_bytebuf_can_read(&buf) == 6);

    sc_bytebuf_write(&buf, (uint8_t *) "hello ", sizeof("hello ") - 1);
    assert(sc_bytebuf_can_read(&buf) == 12);

    sc_bytebuf_prepare_write(&buf, (uint8_t *) "hello ", sizeof("hello ") - 1);
    assert(sc_bytebuf_can_read(&buf) == 12); // write not committed yet

    sc_bytebuf_read(&buf, data, 9);
    assert(!strncmp((char *) data, "hello hel", 3));
    assert(sc_bytebuf_can_read(&buf) == 3);

    sc_bytebuf_commit_write(&buf, sizeof("hello ") - 1);
    assert(sc_bytebuf_can_read(&buf) == 9);

    sc_bytebuf_prepare_write(&buf, (uint8_t *) "world", sizeof("world") - 1);
    assert(sc_bytebuf_can_read(&buf) == 9); // write not committed yet

    sc_bytebuf_commit_write(&buf, sizeof("world") - 1);
    assert(sc_bytebuf_can_read(&buf) == 14);

    sc_bytebuf_write(&buf, (uint8_t *) "!", 1);
    assert(sc_bytebuf_can_read(&buf) == 15);

    sc_bytebuf_skip(&buf, 3);
    assert(sc_bytebuf_can_read(&buf) == 12);

    sc_bytebuf_read(&buf, data, 12);
    data[12] = '\0';
    assert(!strcmp((char *) data, "hello world!"));
    assert(sc_bytebuf_can_read(&buf) == 0);

    sc_bytebuf_destroy(&buf);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_bytebuf_simple();
    test_bytebuf_boundaries();
    test_bytebuf_two_steps_write();

    return 0;
}
