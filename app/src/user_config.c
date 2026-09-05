#include "user_config.h"

#ifndef _WIN32

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool
sc_user_config_ensure_dir(void) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return false;
    }

    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/.scrcpy-auto", home);
    if (n <= 0 || (size_t) n >= sizeof(path)) {
        fprintf(stderr, "scrcpy-auto: user config path is too long\n");
        return false;
    }

    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true;
        }
        fprintf(stderr,
                "scrcpy-auto: user config path exists but is not a directory: "
                "%s\n",
                path);
        return false;
    }

    if (errno != ENOENT) {
        fprintf(stderr, "scrcpy-auto: cannot inspect user config path %s: %s\n",
                path, strerror(errno));
        return false;
    }

    if (mkdir(path, 0700) == 0) {
        return true;
    }

    int mkdir_error = errno;

    // Another process may have created it between stat() and mkdir().
    if (mkdir_error == EEXIST && stat(path, &st) == 0
            && S_ISDIR(st.st_mode)) {
        return true;
    }

    if (mkdir_error == EEXIST) {
        fprintf(stderr,
                "scrcpy-auto: user config path exists but is not a directory: "
                "%s\n",
                path);
    } else {
        fprintf(stderr, "scrcpy-auto: cannot create user config directory %s: "
                        "%s\n",
                path, strerror(mkdir_error));
    }
    return false;
}

#else

bool
sc_user_config_ensure_dir(void) {
    return true;
}

#endif
