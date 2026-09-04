#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/config_file.h"

#define TEST_CONFIG_PATH "test-config-file.ini"

static void
write_config(const char *content) {
    FILE *file = fopen(TEST_CONFIG_PATH, "wb");
    assert(file);
    size_t len = strlen(content);
    assert(fwrite(content, 1, len, file) == len);
    assert(!fclose(file));
}

static void
test_load(void) {
    write_config(
        "\xef\xbb\xbf  max-size = 1024 # default\r\n"
        "background-color = #abc # dark gray\n"
        "no-audio # disabled\n"
        "window-title = My phone  \n"
        "# ignored\n"
        "\n");

    char *argv[] = {
        "scrcpy",
        "--config-file", TEST_CONFIG_PATH,
        "--max-size=2048",
    };
    struct sc_config_argv ca;
    assert(sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                               false));
    assert(ca.argc == 8);
    assert(!strcmp(ca.argv[0], "scrcpy"));
    assert(!strcmp(ca.argv[1], "--max-size=1024"));
    assert(!strcmp(ca.argv[2], "--background-color=#abc"));
    assert(!strcmp(ca.argv[3], "--no-audio"));
    assert(!strcmp(ca.argv[4], "--window-title=My phone"));
    assert(ca.argv[5] == argv[1]);
    assert(ca.argv[6] == argv[2]);
    assert(ca.argv[7] == argv[3]);
    assert(!ca.argv[8]);

    sc_config_argv_destroy(&ca);
    sc_config_argv_destroy(&ca);
}

static void
test_no_config(void) {
    char *argv[] = {
        "scrcpy",
        "--config-file", TEST_CONFIG_PATH,
        "--no-config",
    };
    struct sc_config_argv ca;
    assert(sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                               true));
    assert(ca.argc == (int) ARRAY_LEN(argv));
    assert(ca.argv == argv);
    sc_config_argv_destroy(&ca);
}

static void
test_empty(void) {
    write_config("");
    char *argv[] = {"scrcpy", "--config-file=" TEST_CONFIG_PATH};
    struct sc_config_argv ca;
    assert(sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                               false));
    assert(ca.argc == (int) ARRAY_LEN(argv));
    assert(ca.argv == argv);
    sc_config_argv_destroy(&ca);
}

static void
test_long_line(void) {
    char content[5000];
    memset(content, 'a', sizeof(content) - 1);
    content[sizeof(content) - 1] = '\0';
    write_config(content);

    char *argv[] = {"scrcpy", "--config-file=" TEST_CONFIG_PATH};
    struct sc_config_argv ca;
    assert(!sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                                false));
    sc_config_argv_destroy(&ca);
}

static void
test_missing_explicit_file(void) {
    remove(TEST_CONFIG_PATH);
    char *argv[] = {"scrcpy", "--config-file=" TEST_CONFIG_PATH};
    struct sc_config_argv ca;
    assert(!sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                                false));
    sc_config_argv_destroy(&ca);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_load();
    test_no_config();
    test_empty();
    test_long_line();
    test_missing_explicit_file();
    remove(TEST_CONFIG_PATH);
    return 0;
}
