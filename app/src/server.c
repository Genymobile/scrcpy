#include "command.h"

#include <SDL2/SDL_log.h>
#include <errno.h>

process_t start_server(const char *serial) {
    const char *const cmd[] = {
        "shell",
        "CLASSPATH=/data/local/tmp/scrcpy-server.jar",
        "app_process",
        "/system/bin",
        "com.genymobile.scrcpy.ScrCpyServer"
    };
    return adb_execute(serial, cmd, sizeof(cmd) / sizeof(cmd[0]));
}

void stop_server(process_t server) {
    if (!cmd_terminate(server)) {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Could not terminate server: %s", strerror(errno));
    }
}
