#ifndef STRUTIL_H
#define STRUTIL_H

#include <stddef.h>

// like strncpy, except:
//  - it copies at most n-1 chars
//  - the dest string is nul-terminated
//  - it does not write useless bytes if strlen(src) < n
//  - it returns the number of chars actually written (max n-1) if src has
//    been copied completely, or n if src has been truncated
size_t xstrncpy(char *dest, const char *src, size_t n);

// join tokens by sep into dst
// returns the number of chars actually written (max n-1) if no trucation
// occurred, or n if truncated
size_t xstrjoin(char *dst, const char *const tokens[], char sep, size_t n);

#endif
