#include "util/file.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
# include <mach-o/dyld.h> // for _NSGetExecutablePath()
#endif

#include "util/log.h"

bool
sc_file_executable_exists(const char *file) {
    char *path = getenv("PATH");
    if (!path)
        return false;
    path = strdup(path);
    if (!path)
        return false;

    bool ret = false;
    size_t file_len = strlen(file);
    char *saveptr;
    for (char *dir = strtok_r(path, ":", &saveptr); dir;
            dir = strtok_r(NULL, ":", &saveptr)) {
        size_t dir_len = strlen(dir);
        char *fullpath = malloc(dir_len + file_len + 2);
        if (!fullpath)
        {
            LOG_OOM();
            continue;
        }
        memcpy(fullpath, dir, dir_len);
        fullpath[dir_len] = '/';
        memcpy(fullpath + dir_len + 1, file, file_len + 1);

        struct stat sb;
        bool fullpath_executable = stat(fullpath, &sb) == 0 &&
            sb.st_mode & S_IXUSR;
        free(fullpath);
        if (fullpath_executable) {
            ret = true;
            break;
        }
    }

    free(path);
    return ret;
}

char *
sc_file_get_executable_path(void) {
// <https://stackoverflow.com/a/1024937/1987178>
#ifdef __linux__
    char buf[PATH_MAX + 1]; // +1 for the null byte
    ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
    if (len == -1) {
        perror("readlink");
        return NULL;
    }
    buf[len] = '\0';
    return strdup(buf);
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    if (_NSGetExecutablePath(buf, &bufsize) != 0) {
        LOGE("Executable path buffer too small; need %u bytes", bufsize);
        return NULL;
    }
    return realpath(buf, NULL);
#else
    // "_" is often used to store the full path of the command being executed
    char *path = getenv("_");
    if (!path) {
        LOGE("Could not determine executable path");
        return NULL;
    }
    return strdup(path);
#endif
}

bool
sc_file_is_regular(const char *path) {
    struct stat path_stat;

    if (stat(path, &path_stat)) {
        perror("stat");
        return false;
    }
    return S_ISREG(path_stat.st_mode);
}

