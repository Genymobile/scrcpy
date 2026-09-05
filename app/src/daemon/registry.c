#include "registry.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
# include <direct.h>
# include <process.h>
# include <windows.h>
#else
# include <sys/stat.h>
# include <sys/types.h>
# include <unistd.h>
#endif

#include "daemon/protocol.h"
#include "util/env.h"
#include "util/log.h"
#include "util/strbuf.h"

static bool
make_dir(const char *path) {
#ifdef _WIN32
    int r = _mkdir(path);
#else
    int r = mkdir(path, 0700);
#endif
    return r == 0 || errno == EEXIST;
}

// Returns the registry directory (malloc'd), creating it if necessary,
// or NULL on error
static char *
get_registry_dir(void) {
    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 128)) {
        return NULL;
    }

#ifdef _WIN32
    char *base = sc_get_env("LOCALAPPDATA");
    if (!base) {
        goto error;
    }
    bool ok = sc_strbuf_append_str(&buf, base)
           && sc_strbuf_append_staticstr(&buf, "\\scrcpy-auto");
    free(base);
    if (!ok || !make_dir(buf.s)) {
        goto error;
    }
    if (!sc_strbuf_append_staticstr(&buf, "\\run")) {
        goto error;
    }
#else
    char *base = sc_get_env("XDG_RUNTIME_DIR");
    if (base) {
        bool ok = sc_strbuf_append_str(&buf, base)
               && sc_strbuf_append_staticstr(&buf, "/scrcpy-auto");
        free(base);
        if (!ok) {
            goto error;
        }
    } else {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "/tmp/scrcpy-auto-%ld", (long) getuid());
        if (!sc_strbuf_append_str(&buf, tmp)) {
            goto error;
        }
    }
#endif

    if (!make_dir(buf.s)) {
        LOGW("Could not create registry directory: %s", buf.s);
        goto error;
    }

    return buf.s;

error:
    free(buf.s);
    return NULL;
}

// Returns the registry entry path (malloc'd) for `port`, or NULL
static char *
get_entry_path(uint16_t port, const char *suffix) {
    char *dir = get_registry_dir();
    if (!dir) {
        return NULL;
    }

    char *path;
    int r = asprintf(&path, "%s%c%u.json%s", dir,
#ifdef _WIN32
                     '\\',
#else
                     '/',
#endif
                     (unsigned) port, suffix);
    free(dir);
    if (r == -1) {
        LOG_OOM();
        return NULL;
    }
    return path;
}

static uint32_t
get_pid(void) {
#ifdef _WIN32
    return (uint32_t) GetCurrentProcessId();
#else
    return (uint32_t) getpid();
#endif
}

bool
sc_registry_write(uint16_t port, const char *serial, const char *device_name,
                  const char *state) {
    char *path = get_entry_path(port, "");
    if (!path) {
        return false;
    }
    char *tmp_path = get_entry_path(port, ".tmp");
    if (!tmp_path) {
        free(path);
        return false;
    }

    bool ok = false;

    char timestamp[32] = "";
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    if (tm) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm);
    }

    struct sc_strbuf buf;
    if (!sc_strbuf_init(&buf, 256)) {
        goto end;
    }

    char head[128];
    snprintf(head, sizeof(head),
             "{\"registry_version\":%d,\"port\":%u,\"pid\":%lu,",
             SC_REGISTRY_VERSION, (unsigned) port, (unsigned long) get_pid());

    bool w = sc_strbuf_append_str(&buf, head)
          && sc_strbuf_append_staticstr(&buf, "\"serial\":")
          && sc_json_append_escaped(&buf, serial ? serial : "")
          && sc_strbuf_append_staticstr(&buf, ",\"device_name\":")
          && sc_json_append_escaped(&buf, device_name ? device_name : "")
          && sc_strbuf_append_staticstr(&buf, ",\"state\":")
          && sc_json_append_escaped(&buf, state ? state : "")
          && sc_strbuf_append_staticstr(&buf, ",\"started_at\":")
          && sc_json_append_escaped(&buf, timestamp);

    char tail[48];
    snprintf(tail, sizeof(tail), ",\"protocol\":%d}\n",
             SC_DAEMON_PROTOCOL_VERSION);
    w = w && sc_strbuf_append_str(&buf, tail);

    if (!w) {
        free(buf.s);
        goto end;
    }

    FILE *fp = fopen(tmp_path, "w");
    if (!fp) {
        LOGW("Could not write registry entry: %s", tmp_path);
        free(buf.s);
        goto end;
    }
    size_t written = fwrite(buf.s, 1, buf.len, fp);
    fclose(fp);
    free(buf.s);
    if (written != buf.len) {
        remove(tmp_path);
        goto end;
    }

#ifdef _WIN32
    // rename() fails on Windows if the target exists
    remove(path);
#endif
    if (rename(tmp_path, path)) {
        LOGW("Could not update registry entry: %s", path);
        remove(tmp_path);
        goto end;
    }

    ok = true;

end:
    free(tmp_path);
    free(path);
    return ok;
}

void
sc_registry_remove(uint16_t port) {
    char *path = get_entry_path(port, "");
    if (!path) {
        return;
    }
    remove(path);
    free(path);
}
