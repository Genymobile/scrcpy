#include "command.h"

#include <SDL2/SDL_log.h>
#include <errno.h>

#define SOCKET_NAME "scrcpy"

process_t push_server(const char *serial) {
    const char *apk_path = getenv("SCRCPY_APK");
    if (!apk_path) {
        apk_path = "scrcpy.apk";
    }
    return adb_push(serial, apk_path, "/data/local/tmp/");
}

process_t enable_tunnel(const char *serial, Uint16 local_port) {
    return adb_reverse(serial, SOCKET_NAME, local_port);
}

process_t disable_tunnel(const char *serial) {
    return adb_reverse_remove(serial, SOCKET_NAME);
}

process_t start_server(const char *serial, Uint16 max_size) {
    char max_size_string[6];
    sprintf(max_size_string, "%d", max_size);
    const char *const cmd[] = {
        "shell",
        "CLASSPATH=/data/local/tmp/scrcpy.apk",
        "app_process",
        "/", // unused
        "com.genymobile.scrcpy.ScrCpyServer",
        max_size_string,
    };
    return adb_execute(serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

void stop_server(process_t server) {
    if (!cmd_terminate(server)) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Could not terminate server: %s", strerror(errno));
    }
}
