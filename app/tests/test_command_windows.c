#include "common.h"

#include <assert.h>
#include <stdlib.h>

#include "util/command.h"

static void test_command_with_spaces(void) {
    const char *const argv[] = {
        "C:\\Program Files\\scrcpy\\adb",
        "-s",
        "serial with spaces",
        "push",
        "E:\\some folder\\scrcpy-server",
        "/data/local/tmp/scrcpy-server.jar",
        NULL,
    };
    char *cmd = sc_command_serialize_windows(argv);
    const char *expected = "\"C:\\Program Files\\scrcpy\\adb\" "
                           "\"-s\" "
                           "\"serial with spaces\" "
                           "\"push\" "
                           "\"E:\\some folder\\scrcpy-server\" "
                           "\"/data/local/tmp/scrcpy-server.jar\"";

    assert(!strcmp(expected, cmd));
    free(cmd);
}

static void test_command_with_backslashes(void) {
    const char *const argv[] = {
        "a\\\\ b\\",
        "def \\",
        "gh\"i\" \\\\",
        "jkl\\\\",
        "mno\\",
        "p\\\"qr",
        NULL,
    };

    char *cmd = sc_command_serialize_windows(argv);
    const char *expected = "\"a\\\\ b\\\\\" "
                           "\"def \\\\\" "
                           "\"gh\\\"i\\\" \\\\\\\\\" "
                           "\"jkl\\\\\\\\\" "
                           "\"mno\\\\\" "
                           "\"p\\\\\\\"qr\"";

    assert(!strcmp(expected, cmd));
    free(cmd);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_command_with_spaces();
    test_command_with_backslashes();

    return 0;
}
