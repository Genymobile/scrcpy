#include "config_file.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/env.h"
#include "util/file.h"
#include "util/log.h"
#include "util/vector.h"
#ifdef _WIN32
# include "util/str.h"
#endif

#define SC_CONFIG_FILENAME "config.ini"
#define SC_CONFIG_LINE_MAX 4096

struct sc_config_args SC_VECTOR(char *);

static char *
sc_config_get_default_path(void) {
#ifdef PORTABLE
    return sc_file_get_local_path(SC_CONFIG_FILENAME);
#else
# ifdef _WIN32
    char *base = sc_get_env("APPDATA");
    if (!base || !*base) {
        free(base);
        return NULL;
    }
# else
    char *base = sc_get_env("XDG_CONFIG_HOME");
    if (base && !*base) {
        free(base);
        base = NULL;
    }
    if (!base) {
        char *home = sc_get_env("HOME");
        if (!home || !*home) {
            free(home);
            return NULL;
        }
        base = sc_file_build_path(home, ".config");
        free(home);
        if (!base) {
            return NULL;
        }
    }
# endif

    char *dir = sc_file_build_path(base, "scrcpy");
    free(base);
    if (!dir) {
        return NULL;
    }

    char *path = sc_file_build_path(dir, SC_CONFIG_FILENAME);
    free(dir);
    return path;
#endif
}

static bool
sc_config_get_path(const char *cli_path, bool disabled, char **path,
                   bool *required) {
    if (disabled) {
        *path = NULL;
        *required = false;
        return true;
    }

    if (cli_path) {
        *path = strdup(cli_path);
        if (!*path) {
            LOG_OOM();
            return false;
        }
        *required = true;
        return true;
    }

    char *env_path = sc_get_env("SCRCPY_CONFIG_FILE");
    if (env_path && *env_path) {
        *path = env_path;
        *required = true;
        return true;
    }
    free(env_path);

    *path = sc_config_get_default_path();
    *required = false;
    return true;
}

static FILE *
sc_config_open(const char *path) {
#ifdef _WIN32
    wchar_t *wide_path = sc_str_to_wchars(path);
    if (!wide_path) {
        errno = EINVAL;
        return NULL;
    }

    FILE *file = _wfopen(wide_path, L"rb");
    free(wide_path);
    return file;
#else
    return fopen(path, "rb");
#endif
}

static char *
sc_config_skip_spaces(char *s) {
    while (*s == ' ' || *s == '\t') {
        ++s;
    }
    return s;
}

static void
sc_config_trim_right(char *s) {
    size_t len = strlen(s);
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static void
sc_config_strip_inline_comment(char *s) {
    if (!*s) {
        return;
    }

    char *p = s + 1;
    while ((p = strchr(p, '#'))) {
        if (p[-1] == ' ' || p[-1] == '\t') {
            *p = '\0';
            return;
        }
        ++p;
    }
}

static bool
sc_config_create_arg(char *line, const char *path, size_t line_number,
                     char **out) {
    char *key = sc_config_skip_spaces(line);
    if (!*key || *key == '#') {
        *out = NULL;
        return true;
    }

    char *separator = strchr(key, '=');
    char *value = NULL;
    if (separator) {
        *separator = '\0';
        value = sc_config_skip_spaces(separator + 1);
        sc_config_strip_inline_comment(value);
        sc_config_trim_right(value);
    } else {
        sc_config_strip_inline_comment(key);
    }
    sc_config_trim_right(key);

    if (!*key) {
        LOGE("Invalid configuration option in %s:%zu", path, line_number);
        return false;
    }

    size_t key_len = strlen(key);
    size_t value_len = value ? strlen(value) : 0;
    size_t arg_len = 2 + key_len + (value ? 1 + value_len : 0);
    char *arg = malloc(arg_len + 1);
    if (!arg) {
        LOG_OOM();
        return false;
    }

    arg[0] = '-';
    arg[1] = '-';
    memcpy(arg + 2, key, key_len);
    size_t offset = 2 + key_len;
    if (value) {
        arg[offset++] = '=';
        memcpy(arg + offset, value, value_len);
        offset += value_len;
    }
    arg[offset] = '\0';
    *out = arg;
    return true;
}

static void
sc_config_args_destroy(struct sc_config_args *args) {
    for (size_t i = 0; i < args->size; ++i) {
        free(args->data[i]);
    }
    sc_vector_destroy(args);
}

static bool
sc_config_parse(FILE *file, const char *path, struct sc_config_args *args) {
    char line[SC_CONFIG_LINE_MAX];
    size_t line_number = 0;

    while (fgets(line, sizeof(line), file)) {
        ++line_number;
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') {
            line[--len] = '\0';
        } else if (!feof(file)) {
            LOGE("Configuration line too long in %s:%zu", path, line_number);
            return false;
        }
        if (len && line[len - 1] == '\r') {
            line[--len] = '\0';
        }

        char *content = line;
        if (line_number == 1 && len >= 3
                && !memcmp(content, "\xef\xbb\xbf", 3)) {
            content += 3;
        }

        char *arg;
        if (!sc_config_create_arg(content, path, line_number, &arg)) {
            return false;
        }
        if (!arg) {
            continue;
        }

        if (!sc_vector_push(args, arg)) {
            LOG_OOM();
            free(arg);
            return false;
        }
    }

    if (ferror(file)) {
        LOGE("Could not read configuration file %s: %s", path,
             strerror(errno));
        return false;
    }

    return true;
}

bool
sc_config_argv_init(struct sc_config_argv *ca, int argc, char *argv[],
                    const char *config_path, bool config_disabled) {
    *ca = (struct sc_config_argv) {0};

    char *path;
    bool required;
    if (!sc_config_get_path(config_path, config_disabled, &path, &required)) {
        return false;
    }

    if (!path) {
        ca->argv = argv;
        ca->argc = argc;
        return true;
    }

    FILE *file = sc_config_open(path);
    if (!file) {
        int error = errno;
        if (!required && (error == ENOENT || error == ENOTDIR)) {
            free(path);
            ca->argv = argv;
            ca->argc = argc;
            return true;
        }

        LOGE("Could not open configuration file %s: %s", path,
             strerror(error));
        free(path);
        return false;
    }

    struct sc_config_args config_args = {0};
    bool ok = sc_config_parse(file, path, &config_args);
    fclose(file);
    if (!ok) {
        sc_config_args_destroy(&config_args);
        free(path);
        return false;
    }

    LOGD("Loaded %zu configuration option(s) from %s", config_args.size,
         path);
    free(path);

    if (!config_args.size) {
        sc_vector_destroy(&config_args);
        ca->argv = argv;
        ca->argc = argc;
        return true;
    }

    size_t merged_argc = (size_t) argc + config_args.size;
    if (merged_argc > INT_MAX) {
        LOGE("Too many configuration options");
        sc_config_args_destroy(&config_args);
        return false;
    }

    char **merged_argv = malloc((merged_argc + 1) * sizeof(*merged_argv));
    if (!merged_argv) {
        LOG_OOM();
        sc_config_args_destroy(&config_args);
        return false;
    }

    merged_argv[0] = argv[0];
    memcpy(&merged_argv[1], config_args.data,
           config_args.size * sizeof(*merged_argv));
    memcpy(&merged_argv[config_args.size + 1], &argv[1],
           (argc - 1) * sizeof(*merged_argv));
    merged_argv[merged_argc] = NULL;

    ca->argv = merged_argv;
    ca->argc = (int) merged_argc;
    ca->owned_args = config_args.data;
    ca->owned_argc = config_args.size;
    return true;
}

void
sc_config_argv_destroy(struct sc_config_argv *ca) {
    for (size_t i = 0; i < ca->owned_argc; ++i) {
        free(ca->owned_args[i]);
    }
    free(ca->owned_args);
    if (ca->owned_argc) {
        free(ca->argv);
    }
    *ca = (struct sc_config_argv) {0};
}
