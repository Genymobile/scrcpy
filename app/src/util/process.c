#include "process.h"

#include <libgen.h>
#include "log.h"

bool
process_check_success(process_t proc, const char *name, bool close) {
    if (proc == PROCESS_NONE) {
        LOGE("Could not execute \"%s\"", name);
        return false;
    }
    exit_code_t exit_code = process_wait(proc, close);
    if (exit_code) {
        if (exit_code != NO_EXIT_CODE) {
            LOGE("\"%s\" returned with value %" PRIexitcode, name, exit_code);
        } else {
            LOGE("\"%s\" exited unexpectedly", name);
        }
        return false;
    }
    return true;
}

char *
get_local_file_path(const char *name) {
    char *executable_path = get_executable_path();
    if (!executable_path) {
        return NULL;
    }

    // dirname() does not work correctly everywhere, so get the parent
    // directory manually.
    // See <https://github.com/Genymobile/scrcpy/issues/2619>
    char *p = strrchr(executable_path, PATH_SEPARATOR);
    if (!p) {
        LOGE("Unexpected executable path: \"%s\" (it should contain a '%c')",
             executable_path, PATH_SEPARATOR);
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
        LOGE("Could not alloc path");
        free(executable_path);
        return NULL;
    }

    memcpy(file_path, dir, dirlen);
    file_path[dirlen] = PATH_SEPARATOR;
    // namelen + 1 to copy the final '\0'
    memcpy(&file_path[dirlen + 1], name, namelen + 1);

    free(executable_path);

    return file_path;
}
