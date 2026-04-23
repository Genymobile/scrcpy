#include "config_file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/env.h"
#include "util/file.h"
#include "util/log.h"
#include "util/str.h"

#define SC_CONFIG_FILENAME "config.ini"

// Get the default config file path for the current platform.
// Returns a heap-allocated string, or NULL if not found.
static char *
sc_config_get_default_path(void) {
#ifdef PORTABLE
    return sc_file_get_local_path(SC_CONFIG_FILENAME);
#else
# ifdef _WIN32
    char *appdata = sc_get_env("APPDATA");
    if (!appdata) {
        return NULL;
    }
    char *dir = sc_str_concat(appdata, "\\scrcpy");
    free(appdata);
    if (!dir) {
        return NULL;
    }
    char *path = sc_str_concat(dir, "\\" SC_CONFIG_FILENAME);
    free(dir);
    return path;
# else
    // Unix: $XDG_CONFIG_HOME/scrcpy/config.ini
    // fallback: ~/.config/scrcpy/config.ini
    char *base = sc_get_env("XDG_CONFIG_HOME");
    if (!base) {
        char *home = sc_get_env("HOME");
        if (!home) {
            return NULL;
        }
        base = sc_str_concat(home, "/.config");
        free(home);
        if (!base) {
            return NULL;
        }
    }
    char *dir = sc_str_concat(base, "/scrcpy");
    free(base);
    if (!dir) {
        return NULL;
    }
    char *path = sc_str_concat(dir, "/" SC_CONFIG_FILENAME);
    free(dir);
    return path;
# endif
#endif
}

// Resolve config file path: --config-file=PATH > env > default
static char *
sc_config_get_path(int argc, char *argv[]) {
    // Check for --config-file= in CLI args
    for (int i = 1; i < argc; ++i) {
        if (!strncmp(argv[i], "--config-file=", 14)) {
            return strdup(argv[i] + 14);
        }
        if (!strcmp(argv[i], "--config-file") && i + 1 < argc) {
            return strdup(argv[i + 1]);
        }
    }

    // Check env var
    char *env = sc_get_env("SCRCPY_CONFIG_FILE");
    if (env) {
        return env; // already heap-allocated
    }

    return sc_config_get_default_path();
}

// Check if --no-config is present in CLI args
static bool
sc_config_has_no_config(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-config")) {
            return true;
        }
    }
    return false;
}

// Parse config file into an array of "--key=value" or "--key" strings.
// Returns the number of entries written to `out`, or -1 on error.
// Caller must free each out[i].
static int
sc_config_parse_file(const char *path, char **out, int max) {
    FILE *f = fopen(path, "r");
    if (!f) {
        LOGD("Could not open config file: %s: %s", path, strerror(errno));
        return 0; // not an error, just no config
    }

    int count = 0;
    char line[4096];

    while (fgets(line, sizeof(line), f)) {

        // Strip trailing newline/carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        // Strip leading whitespace
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }

        // Skip empty lines and comments
        if (*p == '\0' || *p == '#') {
            continue;
        }

        if (count >= max) {
            LOGW("Config file: too many entries (max %d), ignoring rest", max);
            break;
        }

        // Build "--key=value" or "--key"
        char *eq = strchr(p, '=');
        char *arg;
        if (eq) {
            // key=value → "--key=value"
            *eq = '\0';
            const char *key = p;
            const char *value = eq + 1;

            // "--" + key + "=" + value + '\0'
            size_t arglen = 2 + strlen(key) + 1 + strlen(value) + 1;
            arg = malloc(arglen);
            if (!arg) {
                LOG_OOM();
                fclose(f);
                return -1;
            }
            snprintf(arg, arglen, "--%s=%s", key, value);
        } else {
            // boolean key → "--key"
            // "--" + key + '\0'
            size_t arglen = 2 + strlen(p) + 1;
            arg = malloc(arglen);
            if (!arg) {
                LOG_OOM();
                fclose(f);
                return -1;
            }
            snprintf(arg, arglen, "--%s", p);
        }

        out[count++] = arg;
    }

    fclose(f);

    LOGD("Config file: loaded %d entries from %s", count, path);
    return count;
}

bool
sc_config_argv_init(struct sc_config_argv *ca, int argc, char *argv[]) {
    ca->argv = NULL;
    ca->argc = 0;
    ca->allocs = NULL;
    ca->nallocs = 0;

    if (sc_config_has_no_config(argc, argv)) {
        // No config: just reference original argv
        ca->argv = argv;
        ca->argc = argc;
        return true;
    }

    char *path = sc_config_get_path(argc, argv);
    if (!path || !sc_file_is_regular(path)) {
        free(path);
        // No config file: just use original argv
        ca->argv = argv;
        ca->argc = argc;
        return true;
    }

    // Parse config file entries (max 256 entries)
    #define SC_CONFIG_MAX_ENTRIES 256
    char *config_args[SC_CONFIG_MAX_ENTRIES];
    int nconfig = sc_config_parse_file(path, config_args, SC_CONFIG_MAX_ENTRIES);
    free(path);

    if (nconfig < 0) {
        return false;
    }

    if (nconfig == 0) {
        ca->argv = argv;
        ca->argc = argc;
        return true;
    }

    // Build merged argv: [argv[0]] + config_args + [argv[1:]]
    int merged_argc = 1 + nconfig + (argc - 1);
    char **merged_argv = malloc((merged_argc + 1) * sizeof(char *));
    if (!merged_argv) {
        LOG_OOM();
        for (int i = 0; i < nconfig; ++i) {
            free(config_args[i]);
        }
        return false;
    }

    int idx = 0;
    merged_argv[idx++] = argv[0];
    for (int i = 0; i < nconfig; ++i) {
        merged_argv[idx++] = config_args[i];
    }
    for (int i = 1; i < argc; ++i) {
        merged_argv[idx++] = argv[i];
    }
    merged_argv[idx] = NULL;

    // Save allocations for cleanup
    char **allocs = malloc(nconfig * sizeof(char *));
    if (!allocs) {
        LOG_OOM();
        for (int i = 0; i < nconfig; ++i) {
            free(config_args[i]);
        }
        free(merged_argv);
        return false;
    }
    memcpy(allocs, config_args, nconfig * sizeof(char *));

    ca->argv = merged_argv;
    ca->argc = merged_argc;
    ca->allocs = allocs;
    ca->nallocs = nconfig;

    return true;
}

void
sc_config_argv_destroy(struct sc_config_argv *ca) {
    for (int i = 0; i < ca->nallocs; ++i) {
        free(ca->allocs[i]);
    }
    free(ca->allocs);
    // merged_argv is NULL when we used original argv directly
    if (ca->nallocs > 0) {
        free(ca->argv);
    }
}
