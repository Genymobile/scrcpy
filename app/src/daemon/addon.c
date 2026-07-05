#include "addon.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "util/log.h"

// Query the script for its command name (SC_ADDON_MODE=register). Returns a
// malloc'd name (first non-empty, trimmed line of stdout) or NULL.
static char *
query_name(const char *path) {
    const char *argv[] = {"env", "SC_ADDON_MODE=register", path, NULL};
    sc_pid pid;
    sc_pipe pout;
    enum sc_process_result pr =
        sc_process_execute_p(argv, &pid, 0, NULL, &pout, NULL);
    if (pr != SC_PROCESS_SUCCESS) {
        LOGE("Add-on: could not execute \"%s\" for registration", path);
        return NULL;
    }

    char buf[256];
    ssize_t r = sc_pipe_read_all(pout, buf, sizeof(buf) - 1);
    sc_pipe_close(pout);
    sc_exit_code code = sc_process_wait(pid, true);
    if (code != 0) {
        LOGW("Add-on \"%s\": registration exited with code %d", path,
             (int) code);
    }

    if (r <= 0) {
        LOGE("Add-on \"%s\": registration produced no command name", path);
        return NULL;
    }
    buf[r] = '\0';

    // First line, trimmed
    buf[strcspn(buf, "\r\n")] = '\0';
    char *s = buf;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    size_t e = strlen(s);
    while (e > 0 && (s[e - 1] == ' ' || s[e - 1] == '\t')) {
        s[--e] = '\0';
    }
    if (!*s) {
        LOGE("Add-on \"%s\": empty command name", path);
        return NULL;
    }
    return strdup(s);
}

void
sc_addons_load(struct sc_addons *addons, const char *const *paths,
               unsigned count) {
    addons->count = 0;
    for (unsigned i = 0; i < count && addons->count < SC_MAX_ADDONS; ++i) {
        char *name = query_name(paths[i]);
        if (!name) {
            continue;
        }

        bool dup = false;
        for (unsigned j = 0; j < addons->count; ++j) {
            if (!strcmp(addons->list[j].name, name)) {
                dup = true;
                break;
            }
        }
        if (dup) {
            LOGW("Add-on: command \"%s\" already registered, ignoring %s",
                 name, paths[i]);
            free(name);
            continue;
        }

        char *path = strdup(paths[i]);
        if (!path) {
            LOG_OOM();
            free(name);
            continue;
        }
        addons->list[addons->count].name = name;
        addons->list[addons->count].path = path;
        addons->count++;
        LOGI("Add-on loaded: --%s -> %s", name, paths[i]);
    }
}

const char *
sc_addons_find(const struct sc_addons *addons, const char *name) {
    for (unsigned i = 0; i < addons->count; ++i) {
        if (!strcmp(addons->list[i].name, name)) {
            return addons->list[i].path;
        }
    }
    return NULL;
}

void
sc_addons_destroy(struct sc_addons *addons) {
    for (unsigned i = 0; i < addons->count; ++i) {
        free(addons->list[i].name);
        free(addons->list[i].path);
    }
    addons->count = 0;
}

bool
sc_addon_run(const char *path, const char *args, const char *const env[],
             unsigned env_count, sc_pid *pid) {
    // argv: "env" + <env...> + "SC_ADDON_MODE=run" + path + args + NULL
    const char **argv = malloc((env_count + 5) * sizeof(*argv));
    if (!argv) {
        LOG_OOM();
        return false;
    }
    unsigned n = 0;
    argv[n++] = "env";
    for (unsigned i = 0; i < env_count; ++i) {
        argv[n++] = env[i];
    }
    argv[n++] = "SC_ADDON_MODE=run";
    argv[n++] = path;
    argv[n++] = args ? args : "";
    argv[n] = NULL;

    enum sc_process_result pr =
        sc_process_execute((const char *const *) argv, pid, 0);
    free(argv);
    if (pr != SC_PROCESS_SUCCESS) {
        LOGE("Add-on: could not run \"%s\"", path);
        return false;
    }
    return true;
}
