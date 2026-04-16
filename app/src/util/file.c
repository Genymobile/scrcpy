#include "file.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

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

void
sc_file_mkdirs(const char *path) {
    char buf[PATH_MAX];
    size_t n = strlen(path);
    if (n >= sizeof(buf)) {
        return;
    }
    memcpy(buf, path, n + 1);
    for (char *p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0755);
            *p = '/';
        }
    }
    mkdir(buf, 0755);
}

static char *
sc__expand_env_vars(const char *input) {
    size_t len = strlen(input);
    // Allocate a buffer reasonably larger; grow dynamically if needed
    size_t cap = len + 64;
    char *out = malloc(cap);
    if (!out) {
        LOG_OOM();
        return NULL;
    }
    size_t oi = 0;
    for (size_t i = 0; i < len; ) {
        if (input[i] == '$') {
            const char *name = NULL;
            size_t nlen = 0;
            if (i + 1 < len && input[i + 1] == '{') {
                size_t j = i + 2;
                while (j < len && input[j] != '}') {
                    j++;
                }
                if (j < len && input[j] == '}') {
                    name = input + i + 2;
                    nlen = j - (i + 2);
                    i = j + 1;
                } else {
                    // No closing '}', copy literally
                    if (oi + 1 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) { LOG_OOM(); return NULL; } }
                    out[oi++] = input[i++];
                    continue;
                }
            } else {
                size_t j = i + 1;
                while (j < len && ((input[j] >= 'A' && input[j] <= 'Z') || (input[j] >= 'a' && input[j] <= 'z') || (input[j] >= '0' && input[j] <= '9') || input[j] == '_')) {
                    j++;
                }
                if (j > i + 1) {
                    name = input + i + 1;
                    nlen = j - (i + 1);
                    i = j;
                } else {
                    if (oi + 1 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) { LOG_OOM(); return NULL; } }
                    out[oi++] = input[i++];
                    continue;
                }
            }
            char varname[256];
            if (nlen >= sizeof(varname)) nlen = sizeof(varname) - 1;
            memcpy(varname, name, nlen);
            varname[nlen] = '\0';
            const char *val = getenv(varname);
            if (!val) val = "";
            size_t vlen = strlen(val);
            if (oi + vlen + 1 >= cap) { while (oi + vlen + 1 >= cap) cap *= 2; out = realloc(out, cap); if (!out) { LOG_OOM(); return NULL; } }
            memcpy(out + oi, val, vlen);
            oi += vlen;
        } else {
            if (oi + 1 >= cap) { cap *= 2; out = realloc(out, cap); if (!out) { LOG_OOM(); return NULL; } }
            out[oi++] = input[i++];
        }
    }
    out[oi] = '\0';
    return out;
}

char *
sc_file_expand_path(const char *path) {
    if (!path) {
        return NULL;
    }
    if (strcmp(path, "tmpDir") == 0) {
        return strdup(path);
    }
    // Step 1: expand leading ~ or ~/...
    char *tilde_expanded = NULL;
    if (path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "";
        size_t home_len = strlen(home);
        size_t rest_len = strlen(path + 1);
        size_t len = home_len + rest_len + 1;
        tilde_expanded = malloc(len);
        if (!tilde_expanded) { LOG_OOM(); return NULL; }
        memcpy(tilde_expanded, home, home_len);
        memcpy(tilde_expanded + home_len, path + 1, rest_len + 1);
    } else {
        tilde_expanded = strdup(path);
        if (!tilde_expanded) { LOG_OOM(); return NULL; }
    }

    // Step 2: expand environment variables
    char *env_expanded = sc__expand_env_vars(tilde_expanded);
    free(tilde_expanded);
    if (!env_expanded) {
        return NULL;
    }

    // Step 3: if relative, turn into absolute using getcwd()
    char *result = NULL;
    if (env_expanded[0] == '/') {
        result = env_expanded; // already absolute
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            // If getcwd fails, return the env_expanded as-is
            return env_expanded;
        }
        size_t cwd_len = strlen(cwd);
        size_t add_slash = cwd_len > 0 && cwd[cwd_len - 1] == '/' ? 0 : 1;
        size_t plen = strlen(env_expanded);
        size_t total = cwd_len + add_slash + plen + 1;
        result = malloc(total);
        if (!result) { LOG_OOM(); free(env_expanded); return NULL; }
        memcpy(result, cwd, cwd_len);
        size_t pos = cwd_len;
        if (add_slash) result[pos++] = '/';
        memcpy(result + pos, env_expanded, plen + 1);
        free(env_expanded);
    }

    return result;
}

