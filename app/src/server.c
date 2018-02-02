#include "command.h"

#include <SDL2/SDL_log.h>
#include <errno.h>
#include <stdint.h>

#define SOCKET_NAME "scrcpy"

process_t push_server(const char *serial) {
    const char *server_path = getenv("SCRCPY_SERVER_JAR");
    if (!server_path) {
        server_path = "scrcpy-server.jar";
    }
    return adb_push(serial, server_path, "/data/local/tmp/scrcpy-server.jar");
}

process_t enable_tunnel(const char *serial, Uint16 local_port) {
    return adb_reverse(serial, SOCKET_NAME, local_port);
}

process_t disable_tunnel(const char *serial) {
    return adb_reverse_remove(serial, SOCKET_NAME);
}

process_t start_server(const char *serial, Uint16 max_size, Uint32 bit_rate) {
    char max_size_string[6];
    char bit_rate_string[11];
    sprintf(max_size_string, "%"PRIu16, max_size);
    sprintf(bit_rate_string, "%"PRIu32, bit_rate);
    const char *const cmd[] = {
        "shell",
        "CLASSPATH=/data/local/tmp/scrcpy-server.jar",
        "app_process",
        "/", // unused
        "com.genymobile.scrcpy.ScrCpyServer",
        max_size_string,
        bit_rate_string,
    };
    return adb_execute(serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

void stop_server(process_t server) {
    if (!cmd_terminate(server)) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Could not terminate server: %s", strerror(errno));
    }
}
