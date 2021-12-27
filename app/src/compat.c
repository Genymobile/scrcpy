#include "compat.h"

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifndef HAVE_STRDUP
char *strdup(const char *s) {
    size_t size = strlen(s) + 1;
    char *dup = malloc(size);
    if (dup) {
        memcpy(dup, s, size);
    }
    return dup;
}
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    int ret = vasprintf(strp, fmt, va);
    va_end(va);
    return ret;
}
#endif

#ifndef HAVE_VASPRINTF
int vasprintf(char **strp, const char *fmt, va_list ap) {
    va_list va;
    va_copy(va, ap);
    int len = vsnprintf(NULL, 0, fmt, va);
    va_end(va);

    char *str = malloc(len + 1);
    if (!str) {
        return -1;
    }

    va_copy(va, ap);
    int len2 = vsnprintf(str, len + 1, fmt, va);
    (void) len2;
    assert(len == len2);
    va_end(va);

    *strp = str;
    return len;
}
#endif
