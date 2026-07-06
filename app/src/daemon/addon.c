#include "addon.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "util/log.h"

// Trim leading/trailing spaces and tabs in place; returns a pointer into `s`.
static char *
trim(char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    size_t e = strlen(s);
    while (e > 0 && (s[e - 1] == ' ' || s[e - 1] == '\t')) {
        s[--e] = '\0';
    }
    return s;
}

// Parse an "arg=" line body: "<name>:<string|list|path|pathlist>:<req>".
static bool
parse_arg(char *spec, struct sc_addon_arg *out) {
    char *saveptr = NULL;
    char *name = strtok_r(spec, ":", &saveptr);
    if (!name || !*name) {
        return false;
    }
    char *type = strtok_r(NULL, ":", &saveptr);
    char *req = strtok_r(NULL, ":", &saveptr);

    out->name = strdup(name);
    if (!out->name) {
        LOG_OOM();
        return false;
    }
    // type in {string, list, path, pathlist}; default string
    out->is_list = type && (!strcmp(type, "list") || !strcmp(type, "pathlist"));
    out->is_path = type && (!strcmp(type, "path") || !strcmp(type, "pathlist"));
    out->required = req && !strcmp(req, "required");
    return true;
}

// Parse an "env=" line body: "<NAME>:<required|optional>[:<description>]".
static bool
parse_env(char *spec, struct sc_addon_env *out) {
    char *saveptr = NULL;
    char *name = strtok_r(spec, ":", &saveptr);
    if (!name || !*name) {
        return false;
    }
    char *req = strtok_r(NULL, ":", &saveptr);
    char *desc = strtok_r(NULL, "", &saveptr); // rest of the line (may be NULL)

    out->name = strdup(name);
    if (!out->name) {
        LOG_OOM();
        return false;
    }
    out->required = req && !strcmp(req, "required");
    out->description = desc && *desc ? strdup(desc) : NULL;
    return true;
}

// Parse a "meta=" line body: "<key>:<value>".
static bool
parse_meta(char *spec, struct sc_addon_meta *out) {
    char *colon = strchr(spec, ':');
    if (!colon) {
        return false;
    }
    *colon = '\0';
    char *key = trim(spec);
    if (!*key) {
        return false;
    }
    out->key = strdup(key);
    out->value = strdup(trim(colon + 1));
    if (!out->key || !out->value) {
        LOG_OOM();
        free(out->key);
        free(out->value);
        return false;
    }
    return true;
}

static void
free_addon(struct sc_addon *a) {
    free(a->name);
    free(a->path);
    free(a->result_field);
    for (unsigned i = 0; i < a->arg_count; ++i) {
        free(a->args[i].name);
    }
    for (unsigned i = 0; i < a->env_count; ++i) {
        free(a->envs[i].name);
        free(a->envs[i].description);
    }
    for (unsigned i = 0; i < a->meta_count; ++i) {
        free(a->metas[i].key);
        free(a->metas[i].value);
    }
}

// Query the script (SC_ADDON_MODE=register) and parse its line-based schema
// into `out`. Accepts the keyed form ("name="/"result="/"arg="/"env="/"meta=")
// or a bare command name for back-compat. Unknown keys are ignored. On success
// `out` owns malloc'd strings (out->path stays NULL); on failure `out` is
// zeroed and any partial allocation is freed.
static bool
query_schema(const char *path, struct sc_addon *out) {
    memset(out, 0, sizeof(*out));

    const char *argv[] = {"env", "SC_ADDON_MODE=register", path, NULL};
    sc_pid pid;
    sc_pipe pout;
    enum sc_process_result pr =
        sc_process_execute_p(argv, &pid, 0, NULL, &pout, NULL);
    if (pr != SC_PROCESS_SUCCESS) {
        LOGE("Add-on: could not execute \"%s\" for registration", path);
        return false;
    }

    char buf[2048];
    ssize_t r = sc_pipe_read_all(pout, buf, sizeof(buf) - 1);
    sc_pipe_close(pout);
    sc_exit_code code = sc_process_wait(pid, true);
    if (code != 0) {
        LOGW("Add-on \"%s\": registration exited with code %d", path,
             (int) code);
    }

    if (r <= 0) {
        LOGE("Add-on \"%s\": registration produced no output", path);
        return false;
    }
    buf[r] = '\0';

    char *saveptr = NULL;
    for (char *line = strtok_r(buf, "\r\n", &saveptr); line;
         line = strtok_r(NULL, "\r\n", &saveptr)) {
        line = trim(line);
        if (!*line) {
            continue;
        }
        if (!strncmp(line, "name=", 5)) {
            free(out->name);
            out->name = strdup(trim(line + 5));
            if (!out->name) {
                LOG_OOM();
                goto error;
            }
        } else if (!strncmp(line, "result=", 7)) {
            free(out->result_field);
            out->result_field = strdup(trim(line + 7));
            if (!out->result_field) {
                LOG_OOM();
                goto error;
            }
        } else if (!strncmp(line, "arg=", 4)) {
            if (out->arg_count >= SC_MAX_ADDON_ARGS) {
                LOGW("Add-on \"%s\": too many args (max %d)", path,
                     SC_MAX_ADDON_ARGS);
                continue;
            }
            if (parse_arg(trim(line + 4), &out->args[out->arg_count])) {
                out->arg_count++;
            } else {
                LOGW("Add-on \"%s\": ignoring malformed arg \"%s\"", path, line);
            }
        } else if (!strncmp(line, "env=", 4)) {
            if (out->env_count >= SC_MAX_ADDON_ENVS) {
                LOGW("Add-on \"%s\": too many env decls (max %d)", path,
                     SC_MAX_ADDON_ENVS);
                continue;
            }
            if (parse_env(trim(line + 4), &out->envs[out->env_count])) {
                out->env_count++;
            } else {
                LOGW("Add-on \"%s\": ignoring malformed env \"%s\"", path, line);
            }
        } else if (!strncmp(line, "meta=", 5)) {
            if (out->meta_count >= SC_MAX_ADDON_META) {
                LOGW("Add-on \"%s\": too many meta entries (max %d)", path,
                     SC_MAX_ADDON_META);
                continue;
            }
            if (parse_meta(trim(line + 5), &out->metas[out->meta_count])) {
                out->meta_count++;
            } else {
                LOGW("Add-on \"%s\": ignoring malformed meta \"%s\"", path,
                     line);
            }
        } else if (!out->name && !strchr(line, '=')) {
            // Back-compat: a bare command name on its own line
            out->name = strdup(line);
            if (!out->name) {
                LOG_OOM();
                goto error;
            }
        }
        // Unknown "key=..." lines are ignored (forward compatibility)
    }

    if (!out->name || !*out->name) {
        LOGE("Add-on \"%s\": registration produced no command name", path);
        goto error;
    }
    return true;

error:
    free_addon(out);
    memset(out, 0, sizeof(*out));
    return false;
}

void
sc_addons_load(struct sc_addons *addons, const char *const *paths,
               unsigned count) {
    addons->count = 0;
    for (unsigned i = 0; i < count && addons->count < SC_MAX_ADDONS; ++i) {
        struct sc_addon addon;
        if (!query_schema(paths[i], &addon)) {
            continue;
        }

        bool dup = false;
        for (unsigned j = 0; j < addons->count; ++j) {
            if (!strcmp(addons->list[j].name, addon.name)) {
                dup = true;
                break;
            }
        }
        if (dup) {
            LOGW("Add-on: command \"%s\" already registered, ignoring %s",
                 addon.name, paths[i]);
            free_addon(&addon);
            continue;
        }

        char *path = strdup(paths[i]);
        if (!path) {
            LOG_OOM();
            free_addon(&addon);
            continue;
        }
        addon.path = path;
        addons->list[addons->count++] = addon;
        LOGI("Add-on loaded: --%s -> %s (%u arg%s)", addon.name, paths[i],
             addon.arg_count, addon.arg_count == 1 ? "" : "s");
    }
}

const struct sc_addon *
sc_addons_get(const struct sc_addons *addons, const char *name) {
    for (unsigned i = 0; i < addons->count; ++i) {
        if (!strcmp(addons->list[i].name, name)) {
            return &addons->list[i];
        }
    }
    return NULL;
}

const char *
sc_addons_find(const struct sc_addons *addons, const char *name) {
    const struct sc_addon *a = sc_addons_get(addons, name);
    return a ? a->path : NULL;
}

const char *
sc_addon_missing_env(const struct sc_addon *a) {
    for (unsigned i = 0; i < a->env_count; ++i) {
        if (a->envs[i].required && !getenv(a->envs[i].name)) {
            return a->envs[i].name;
        }
    }
    return NULL;
}

void
sc_addons_destroy(struct sc_addons *addons) {
    for (unsigned i = 0; i < addons->count; ++i) {
        free_addon(&addons->list[i]);
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
