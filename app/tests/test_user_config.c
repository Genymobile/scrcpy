#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "user_config.h"

static void
build_config_path(char *path, size_t size, const char *home) {
    int r = snprintf(path, size, "%s/.scrcpy-auto", home);
    assert(r > 0);
    assert((size_t) r < size);
}

static mode_t
get_mode(const char *path) {
    struct stat st;
    assert(stat(path, &st) == 0);
    return st.st_mode & 0777;
}

static void
test_create_and_reuse(void) {
    char home[] = "/tmp/scrcpy-auto-user-config-XXXXXX";
    assert(mkdtemp(home));
    assert(setenv("HOME", home, 1) == 0);

    char config[sizeof(home) + 32];
    build_config_path(config, sizeof(config), home);

    mode_t previous_umask = umask(0);
    assert(sc_user_config_ensure_dir());
    umask(previous_umask);

    assert(get_mode(config) == 0700);

    char marker[sizeof(config) + 16];
    int r = snprintf(marker, sizeof(marker), "%s/marker", config);
    assert(r > 0);
    assert((size_t) r < sizeof(marker));
    FILE *file = fopen(marker, "w");
    assert(file);
    assert(fclose(file) == 0);

    assert(chmod(config, 0750) == 0);
    assert(sc_user_config_ensure_dir());
    assert(get_mode(config) == 0750);
    assert(access(marker, F_OK) == 0);

    assert(unlink(marker) == 0);
    assert(rmdir(config) == 0);
    assert(rmdir(home) == 0);
}

static void
test_existing_file_is_not_replaced(void) {
    char home[] = "/tmp/scrcpy-auto-user-config-file-XXXXXX";
    assert(mkdtemp(home));
    assert(setenv("HOME", home, 1) == 0);

    char config[sizeof(home) + 32];
    build_config_path(config, sizeof(config), home);

    FILE *file = fopen(config, "w");
    assert(file);
    assert(fclose(file) == 0);

    assert(!sc_user_config_ensure_dir());

    struct stat st;
    assert(stat(config, &st) == 0);
    assert(S_ISREG(st.st_mode));

    assert(unlink(config) == 0);
    assert(rmdir(home) == 0);
}

static void
test_home_unavailable(void) {
    assert(unsetenv("HOME") == 0);
    assert(!sc_user_config_ensure_dir());
}

int
main(void) {
    test_create_and_reuse();
    test_existing_file_is_not_replaced();
    test_home_unavailable();
    return 0;
}
