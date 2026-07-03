#include "file.h"

#include <stdlib.h>
#include <string.h>

#include "util/log.h"

char *
sc_file_build_path(const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);

    size_t len = dir_len + name_len + 2; // +2: '/' and '\0'
    char *path = malloc(len);
    if (!path) {
        LOG_OOM();
        return NULL;
    }

    memcpy(path, dir, dir_len);
    path[dir_len] = SC_PATH_SEPARATOR;
    // namelen + 1 to copy the final '\0'
    memcpy(&path[dir_len + 1], name, name_len + 1);
    return path;
}

char *
sc_file_get_local_path(const char *name) {
    char *executable_path = sc_file_get_executable_path();
    if (!executable_path) {
        return NULL;
    }

    // dirname() does not work correctly everywhere, so get the parent
    // directory manually.
    // See <https://github.com/Genymobile/scrcpy/issues/2619>
    char *p = strrchr(executable_path, SC_PATH_SEPARATOR);
    if (!p) {
        LOGE("Unexpected executable path: \"%s\" (it should contain a '%c')",
             executable_path, SC_PATH_SEPARATOR);
        free(executable_path);
        return NULL;
    }

    *p = '\0'; // modify executable_path in place
    char *dir = executable_path;
    char *file_path = sc_file_build_path(dir, name);

    free(executable_path);

    return file_path;
}
