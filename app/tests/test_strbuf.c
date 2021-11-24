#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/strbuf.h"

static void test_strbuf_simple(void) {
    struct sc_strbuf buf;
    bool ok = sc_strbuf_init(&buf, 10);
    assert(ok);

    ok = sc_strbuf_append_staticstr(&buf, "Hello");
    assert(ok);

    ok = sc_strbuf_append_char(&buf, ' ');
    assert(ok);

    ok = sc_strbuf_append_staticstr(&buf, "world");
    assert(ok);

    ok = sc_strbuf_append_staticstr(&buf, "!\n");
    assert(ok);

    ok = sc_strbuf_append_staticstr(&buf, "This is a test");
    assert(ok);

    ok = sc_strbuf_append_n(&buf, '.', 3);
    assert(ok);

    assert(!strcmp(buf.s, "Hello world!\nThis is a test..."));

    sc_strbuf_shrink(&buf);
    assert(buf.len == buf.cap);
    assert(!strcmp(buf.s, "Hello world!\nThis is a test..."));

    free(buf.s);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_strbuf_simple();
    return 0;
}
