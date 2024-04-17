#include "compat.h"

#include "config.h"

#include <assert.h>
#ifndef HAVE_REALLOCARRAY
# include <errno.h>
#endif
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

#if !defined(HAVE_NRAND48) || !defined(HAVE_JRAND48)
#define SC_RAND48_MASK UINT64_C(0xFFFFFFFFFFFF) // 48 bits
#define SC_RAND48_A UINT64_C(0x5DEECE66D)
#define SC_RAND48_C 0xB
static inline uint64_t rand_iter48(uint64_t x) {
    assert((x & ~SC_RAND48_MASK) == 0);
    return (x * SC_RAND48_A + SC_RAND48_C) & SC_RAND48_MASK;
}

static uint64_t rand_iter48_xsubi(unsigned short xsubi[3]) {
    uint64_t x = ((uint64_t) xsubi[0] << 32)
               | ((uint64_t) xsubi[1] << 16)
               | xsubi[2];

    x = rand_iter48(x);

    xsubi[0] = (x >> 32) & 0XFFFF;
    xsubi[1] = (x >> 16) & 0XFFFF;
    xsubi[2] = x & 0XFFFF;

    return x;
}

#ifndef HAVE_NRAND48
long nrand48(unsigned short xsubi[3]) {
    // range [0, 2^31)
    return rand_iter48_xsubi(xsubi) >> 17;
}
#endif

#ifndef HAVE_JRAND48
long jrand48(unsigned short xsubi[3]) {
    // range [-2^31, 2^31)
    union {
        uint32_t u;
        int32_t i;
    } v;
    v.u = rand_iter48_xsubi(xsubi) >> 16;
    return v.i;
}
#endif
#endif

#ifndef HAVE_REALLOCARRAY
void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    size_t bytes;
    if (__builtin_mul_overflow(nmemb, size, &bytes)) {
      errno = ENOMEM;
      return NULL;
    }
    return realloc(ptr, bytes);
}
#endif
