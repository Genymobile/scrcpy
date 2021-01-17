#include "compat.h"

#include "config.h"

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
