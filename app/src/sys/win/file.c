#include "util/file.h"

#include <windows.h>

#include <sys/stat.h>

#include "util/log.h"
#include "util/str_util.h"

char *
get_executable_path(void) {
    HMODULE hModule = GetModuleHandleW(NULL);
    if (!hModule) {
        return NULL;
    }
    WCHAR buf[MAX_PATH + 1]; // +1 for the null byte
    int len = GetModuleFileNameW(hModule, buf, MAX_PATH);
    if (!len) {
        return NULL;
    }
    buf[len] = '\0';
    return utf8_from_wide_char(buf);
}

bool
is_regular_file(const char *path) {
    wchar_t *wide_path = utf8_to_wide_char(path);
    if (!wide_path) {
        LOGC("Could not allocate wide char string");
        return false;
    }

    struct _stat path_stat;
    int r = _wstat(wide_path, &path_stat);
    free(wide_path);

    if (r) {
        perror("stat");
        return false;
    }
    return S_ISREG(path_stat.st_mode);
}

