#include "str_util.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
# include <windows.h>
# include <tchar.h>
#endif

size_t
xstrncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; ++i)
        dest[i] = src[i];
    if (n)
        dest[i] = '\0';
    return src[i] == '\0' ? i : n;
}

size_t
xstrjoin(char *dst, const char *const tokens[], char sep, size_t n) {
    const char *const *remaining = tokens;
    const char *token = *remaining++;
    size_t i = 0;
    while (token) {
        if (i) {
            dst[i++] = sep;
            if (i == n)
                goto truncated;
        }
        size_t w = xstrncpy(dst + i, token, n - i);
        if (w >= n - i)
            goto truncated;
        i += w;
        token = *remaining++;
    }
    return i;

truncated:
    dst[n - 1] = '\0';
    return n;
}

char *
strquote(const char *src) {
    size_t len = strlen(src);
    char *quoted = malloc(len + 3);
    if (!quoted) {
        return NULL;
    }
    memcpy(&quoted[1], src, len);
    quoted[0] = '"';
    quoted[len + 1] = '"';
    quoted[len + 2] = '\0';
    return quoted;
}

#ifdef _WIN32

wchar_t *
utf8_to_wide_char(const char *utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (!len) {
        return NULL;
    }

    wchar_t *wide = malloc(len * sizeof(wchar_t));
    if (!wide) {
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return wide;
}

#endif
