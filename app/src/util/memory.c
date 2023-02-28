#include "memory.h"

#include <stdlib.h>
#include <errno.h>

void *
sc_allocarray(size_t nmemb, size_t size) {
    size_t bytes;
    if (__builtin_mul_overflow(nmemb, size, &bytes)) {
      errno = ENOMEM;
      return NULL;
    }
    return malloc(bytes);
}
