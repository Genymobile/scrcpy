#include "file.h"

#include <stdlib.h>
#include <string.h>

#include "util/log.h"

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
    size_t dirlen = strlen(dir);
    size_t namelen = strlen(name);

    size_t len = dirlen + namelen + 2; // +2: '/' and '\0'
    char *file_path = malloc(len);
    if (!file_path) {
        LOG_OOM();
        free(executable_path);
        return NULL;
    }

    memcpy(file_path, dir, dirlen);
    file_path[dirlen] = SC_PATH_SEPARATOR;
    // namelen + 1 to copy the final '\0'
    memcpy(&file_path[dirlen + 1], name, namelen + 1);

    free(executable_path);

    return file_path;
}

