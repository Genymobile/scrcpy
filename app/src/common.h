#ifndef COMMON_H
#define COMMON_H

#include "config.h"
#include "compat.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#define MIN(X,Y) (X) < (Y) ? (X) : (Y)
#define MAX(X,Y) (X) > (Y) ? (X) : (Y)

#define container_of(ptr, type, member) \
    ((type *) (((char *) (ptr)) - offsetof(type, member)))

#endif
