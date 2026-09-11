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
                               false, NULL));
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
                               true, NULL));
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
                               false, NULL));
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
                                false, NULL));
    sc_config_argv_destroy(&ca);
}

static void
test_missing_explicit_file(void) {
    remove(TEST_CONFIG_PATH);
    char *argv[] = {"scrcpy", "--config-file=" TEST_CONFIG_PATH};
    struct sc_config_argv ca;
    assert(!sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                                false, NULL));
    sc_config_argv_destroy(&ca);
}

static void
test_profile(void) {
    write_config(
        "max-fps=30\n"
        "[phone]\n"
        "serial=192.168.1.10:5555\n"
        "max-size=1080\n"
        "[tablet] # another device\n"
        "serial=192.168.1.20:5555\n"
        "max-size=1600\n");

    char *argv[] = {
        "scrcpy",
        "phone",
        ("--config-file=" TEST_CONFIG_PATH),
        "--max-size=1200",
    };
    struct sc_config_argv ca;
    assert(sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                               false, "phone"));
    assert(ca.argc == 7);
    assert(!strcmp(ca.argv[1], "--max-fps=30"));
    assert(!strcmp(ca.argv[2], "--serial=192.168.1.10:5555"));
    assert(!strcmp(ca.argv[3], "--max-size=1080"));
    assert(ca.argv[4] == argv[1]);
    assert(ca.argv[5] == argv[2]);
    assert(ca.argv[6] == argv[3]);
    sc_config_argv_destroy(&ca);

    assert(sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                               false, "tablet"));
    assert(ca.argc == 7);
    assert(!strcmp(ca.argv[1], "--max-fps=30"));
    assert(!strcmp(ca.argv[2], "--serial=192.168.1.20:5555"));
    assert(!strcmp(ca.argv[3], "--max-size=1600"));
    sc_config_argv_destroy(&ca);

    assert(sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                               false, NULL));
    assert(ca.argc == 5);
    assert(!strcmp(ca.argv[1], "--max-fps=30"));
    assert(ca.argv[2] == argv[1]);
    sc_config_argv_destroy(&ca);
}

static void
test_invalid_profile(void) {
    write_config("[phone]\nmax-size=1080\n");
    char *argv[] = {"scrcpy", "desktop"};
    struct sc_config_argv ca;
    assert(!sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                                false, "desktop"));
    sc_config_argv_destroy(&ca);

    assert(!sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                                true, "phone"));
    sc_config_argv_destroy(&ca);
}

static void
test_invalid_section(void) {
    char *argv[] = {"scrcpy"};
    struct sc_config_argv ca;

    write_config("[ ]\n");
    assert(!sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                                false, NULL));
    sc_config_argv_destroy(&ca);

    write_config("[phone] trailing\n");
    assert(!sc_config_argv_init(&ca, ARRAY_LEN(argv), argv, TEST_CONFIG_PATH,
                                false, NULL));
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
    test_profile();
    test_invalid_profile();
    test_invalid_section();
    remove(TEST_CONFIG_PATH);
    return 0;
}
