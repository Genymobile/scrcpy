#include <assert.h>
#include <string.h>

#include "str_util.h"

static void test_xstrncpy_simple(void) {
    char s[] = "xxxxxxxxxx";
    size_t w = xstrncpy(s, "abcdef", sizeof(s));

    // returns strlen of copied string
    assert(w == 6);

    // is nul-terminated
    assert(s[6] == '\0');

    // does not write useless bytes
    assert(s[7] == 'x');

    // copies the content as expected
    assert(!strcmp("abcdef", s));
}

static void test_xstrncpy_just_fit(void) {
    char s[] = "xxxxxx";
    size_t w = xstrncpy(s, "abcdef", sizeof(s));

    // returns strlen of copied string
    assert(w == 6);

    // is nul-terminated
    assert(s[6] == '\0');

    // copies the content as expected
    assert(!strcmp("abcdef", s));
}

static void test_xstrncpy_truncated(void) {
    char s[] = "xxx";
    size_t w = xstrncpy(s, "abcdef", sizeof(s));

    // returns 'n' (sizeof(s))
    assert(w == 4);

    // is nul-terminated
    assert(s[3] == '\0');

    // copies the content as expected
    assert(!strncmp("abcdef", s, 3));
}

static void test_xstrjoin_simple(void) {
    const char *const tokens[] = { "abc", "de", "fghi", NULL };
    char s[] = "xxxxxxxxxxxxxx";
    size_t w = xstrjoin(s, tokens, ' ', sizeof(s));

    // returns strlen of concatenation
    assert(w == 11);

    // is nul-terminated
    assert(s[11] == '\0');

    // does not write useless bytes
    assert(s[12] == 'x');

    // copies the content as expected
    assert(!strcmp("abc de fghi", s));
}

static void test_xstrjoin_just_fit(void) {
    const char *const tokens[] = { "abc", "de", "fghi", NULL };
    char s[] = "xxxxxxxxxxx";
    size_t w = xstrjoin(s, tokens, ' ', sizeof(s));

    // returns strlen of concatenation
    assert(w == 11);

    // is nul-terminated
    assert(s[11] == '\0');

    // copies the content as expected
    assert(!strcmp("abc de fghi", s));
}

static void test_xstrjoin_truncated_in_token(void) {
    const char *const tokens[] = { "abc", "de", "fghi", NULL };
    char s[] = "xxxxx";
    size_t w = xstrjoin(s, tokens, ' ', sizeof(s));

    // returns 'n' (sizeof(s))
    assert(w == 6);

    // is nul-terminated
    assert(s[5] == '\0');

    // copies the content as expected
    assert(!strcmp("abc d", s));
}

static void test_xstrjoin_truncated_before_sep(void) {
    const char *const tokens[] = { "abc", "de", "fghi", NULL };
    char s[] = "xxxxxx";
    size_t w = xstrjoin(s, tokens, ' ', sizeof(s));

    // returns 'n' (sizeof(s))
    assert(w == 7);

    // is nul-terminated
    assert(s[6] == '\0');

    // copies the content as expected
    assert(!strcmp("abc de", s));
}

static void test_xstrjoin_truncated_after_sep(void) {
    const char *const tokens[] = { "abc", "de", "fghi", NULL };
    char s[] = "xxxxxxx";
    size_t w = xstrjoin(s, tokens, ' ', sizeof(s));

    // returns 'n' (sizeof(s))
    assert(w == 8);

    // is nul-terminated
    assert(s[7] == '\0');

    // copies the content as expected
    assert(!strcmp("abc de ", s));
}

static void test_utf8_truncate(void) {
    const char *s = "aÉbÔc";
    assert(strlen(s) == 7); // É and Ô are 2 bytes-wide

    size_t count;

    count = utf8_truncation_index(s, 1);
    assert(count == 1);

    count = utf8_truncation_index(s, 2);
    assert(count == 1); // É is 2 bytes-wide

    count = utf8_truncation_index(s, 3);
    assert(count == 3);

    count = utf8_truncation_index(s, 4);
    assert(count == 4);

    count = utf8_truncation_index(s, 5);
    assert(count == 4); // Ô is 2 bytes-wide

    count = utf8_truncation_index(s, 6);
    assert(count == 6);

    count = utf8_truncation_index(s, 7);
    assert(count == 7);

    count = utf8_truncation_index(s, 8);
    assert(count == 7); // no more chars
}

int main(void) {
    test_xstrncpy_simple();
    test_xstrncpy_just_fit();
    test_xstrncpy_truncated();
    test_xstrjoin_simple();
    test_xstrjoin_just_fit();
    test_xstrjoin_truncated_in_token();
    test_xstrjoin_truncated_before_sep();
    test_xstrjoin_truncated_after_sep();
    test_utf8_truncate();
    return 0;
}
