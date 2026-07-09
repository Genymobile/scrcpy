#include "plugins.h"

#include <stdio.h>

#ifndef _WIN32

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util/process.h"

// Run argv to completion (stdio inherited); true iff it started and exited 0.
static bool
run_command(const char *const argv[]) {
    sc_pid pid;
    enum sc_process_result pr = sc_process_execute(argv, &pid, 0);
    if (pr != SC_PROCESS_SUCCESS) {
        fprintf(stderr, "plugins: cannot execute '%s'\n", argv[0]);
        return false;
    }
    return sc_process_wait(pid, true) == 0;
}

// Fill buf with "<HOME>/.scrcpy-auto[/<sub>]"; false on error or truncation.
static bool
scrcpy_auto_path(char *buf, size_t size, const char *sub) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return false;
    }
    int n = sub ? snprintf(buf, size, "%s/.scrcpy-auto/%s", home, sub)
                : snprintf(buf, size, "%s/.scrcpy-auto", home);
    return n > 0 && (size_t) n < size;
}

// Replace <plugins>/<name> with a fresh copy of <clone>/<name>.
static bool
copy_plugin(const char *clone, const char *plugins, const char *name) {
    char src[PATH_MAX];
    char dst[PATH_MAX];
    if (snprintf(src, sizeof(src), "%s/%s", clone, name) >= (int) sizeof(src)
            || snprintf(dst, sizeof(dst), "%s/%s", plugins, name)
                   >= (int) sizeof(dst)) {
        fprintf(stderr, "plugins: path too long for '%s'\n", name);
        return false;
    }
    const char *const rm[] = {"rm", "-rf", dst, NULL};
    run_command(rm); // ignore result: the destination may not exist yet
    const char *const cp[] = {"cp", "-R", src, dst, NULL};
    if (!run_command(cp)) {
        fprintf(stderr, "plugins: failed to copy '%s'\n", name);
        return false;
    }
    return true;
}

// Is <plugins>/<name> an existing directory?
static bool
is_installed(const char *plugins, const char *name) {
    char dst[PATH_MAX];
    if (snprintf(dst, sizeof(dst), "%s/%s", plugins, name)
            >= (int) sizeof(dst)) {
        return false;
    }
    struct stat st;
    return stat(dst, &st) == 0 && S_ISDIR(st.st_mode);
}

// Is <clone>/<name> an existing directory (a candidate plugin)?
static bool
is_plugin_dir(const char *clone, const char *name) {
    char entry[PATH_MAX];
    if (snprintf(entry, sizeof(entry), "%s/%s", clone, name)
            >= (int) sizeof(entry)) {
        return false;
    }
    struct stat st;
    return stat(entry, &st) == 0 && S_ISDIR(st.st_mode);
}

int
sc_plugins_cli(int argc, char *argv[]) {
    bool upgrade = !strcmp(argv[1], "plugins-upgrade");
    const char *action = upgrade ? "upgrade" : "install";

    const char *url = NULL;
    const char *name = NULL;
    for (int i = 2; i < argc; ++i) {
        const char *a = argv[i];
        if (!strncmp(a, "--add-on-name=", 14)) {
            name = a + 14;
        } else if (a[0] == '-') {
            fprintf(stderr, "plugins: unknown option '%s'\n", a);
            return 1;
        } else if (!url) {
            url = a;
        } else {
            fprintf(stderr, "plugins: unexpected argument '%s'\n", a);
            return 1;
        }
    }
    if (!url) {
        fprintf(stderr, "plugins: missing git repository URL\n"
                        "usage: scrcpy-auto plugins-%s <git-url> "
                        "--add-on-name=<name>|ALL\n", action);
        return 1;
    }
    if (!name || !name[0]) {
        fprintf(stderr, "plugins: missing --add-on-name=<name>|ALL\n");
        return 1;
    }

    char plugins[PATH_MAX];
    char clone[PATH_MAX];
    if (!scrcpy_auto_path(plugins, sizeof(plugins), "plugins")
            || !scrcpy_auto_path(clone, sizeof(clone), ".clone-tmp")) {
        fprintf(stderr,
                "plugins: cannot resolve ~/.scrcpy-auto (is HOME set?)\n");
        return 1;
    }

    const char *const mkdir_cmd[] = {"mkdir", "-p", plugins, NULL};
    if (!run_command(mkdir_cmd)) {
        fprintf(stderr, "plugins: cannot create %s\n", plugins);
        return 1;
    }

    // Fresh shallow clone into a scratch dir under ~/.scrcpy-auto.
    const char *const rm_clone[] = {"rm", "-rf", clone, NULL};
    run_command(rm_clone);
    const char *const git[] = {"git", "clone", "--depth", "1", url, clone, NULL};
    if (!run_command(git)) {
        fprintf(stderr, "plugins: git clone failed: %s\n", url);
        run_command(rm_clone);
        return 1;
    }

    int rc = 0;
    unsigned done = 0;
    if (!strcmp(name, "ALL")) {
        DIR *d = opendir(clone);
        if (!d) {
            fprintf(stderr, "plugins: cannot read cloned repository\n");
            rc = 1;
            goto out;
        }
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') {
                continue; // skip ., .., .git and hidden entries
            }
            if (!is_plugin_dir(clone, e->d_name)) {
                continue; // plugins are directories
            }
            // `upgrade ALL` only refreshes plugins already installed here.
            if (upgrade && !is_installed(plugins, e->d_name)) {
                continue;
            }
            if (copy_plugin(clone, plugins, e->d_name)) {
                printf("  %sed %s\n", action, e->d_name);
                ++done;
            } else {
                rc = 1;
            }
        }
        closedir(d);
        if (done == 0 && rc == 0) {
            fprintf(stderr, "plugins: nothing to %s from %s\n", action, url);
            rc = 1;
        }
    } else {
        if (!is_plugin_dir(clone, name)) {
            fprintf(stderr, "plugins: '%s' is not a directory in %s\n", name,
                    url);
            rc = 1;
            goto out;
        }
        if (copy_plugin(clone, plugins, name)) {
            printf("  %sed %s\n", action, name);
            done = 1;
        } else {
            rc = 1;
        }
    }

out:
    run_command(rm_clone);
    if (rc == 0) {
        printf("plugins: %sed %u plugin(s) into %s\n", action, done, plugins);
    }
    return rc;
}

bool
sc_plugins_resolve_addon(const char *spec, char *buf, size_t size) {
    // A filesystem path (contains '/' or names an existing file) is used as-is.
    if (strchr(spec, '/') || access(spec, F_OK) == 0) {
        return false;
    }
    const char *home = getenv("HOME");
    if (!home || !home[0]) {
        return false;
    }
    int n = snprintf(buf, size, "%s/.scrcpy-auto/plugins/%s/entrypoint.sh",
                     home, spec);
    return n > 0 && (size_t) n < size;
}

#else // _WIN32

int
sc_plugins_cli(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    fprintf(stderr, "plugins: plugin management is not supported on Windows\n");
    return 1;
}

bool
sc_plugins_resolve_addon(const char *spec, char *buf, size_t size) {
    (void) spec;
    (void) buf;
    (void) size;
    return false;
}

#endif
