#include "util/file.h"

#include <windows.h>

#include <sys/stat.h>

#include "util/log.h"
#include "util/str.h"

char *
sc_file_get_executable_path(void) {
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
    return sc_str_from_wchars(buf);
}

bool
sc_file_is_regular(const char *path) {
    wchar_t *wide_path = sc_str_to_wchars(path);
    if (!wide_path) {
        LOG_OOM();
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

