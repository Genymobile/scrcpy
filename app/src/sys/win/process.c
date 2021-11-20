#include "util/process.h"

#include <assert.h>
#include <sys/stat.h>

#include "util/log.h"
#include "util/str_util.h"

#define CMD_MAX_LEN 8192

static bool
build_cmd(char *cmd, size_t len, const char *const argv[]) {
    // Windows command-line parsing is WTF:
    // <http://daviddeley.com/autohotkey/parameters/parameters.htm#WINPASS>
    // only make it work for this very specific program
    // (don't handle escaping nor quotes)
    size_t ret = xstrjoin(cmd, argv, ' ', len);
    if (ret >= len) {
        LOGE("Command too long (%" PRIsizet " chars)", len - 1);
        return false;
    }
    return true;
}

enum process_result
process_execute(const char *const argv[], HANDLE *handle) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    char *cmd = malloc(CMD_MAX_LEN);
    if (!cmd || !build_cmd(cmd, CMD_MAX_LEN, argv)) {
        *handle = NULL;
        return PROCESS_ERROR_GENERIC;
    }

    wchar_t *wide = utf8_to_wide_char(cmd);
    free(cmd);
    if (!wide) {
        LOGC("Could not allocate wide char string");
        return PROCESS_ERROR_GENERIC;
    }

    if (!CreateProcessW(NULL, wide, NULL, NULL, FALSE, 0, NULL, NULL, &si,
                        &pi)) {
        free(wide);
        *handle = NULL;
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            return PROCESS_ERROR_MISSING_BINARY;
        }
        return PROCESS_ERROR_GENERIC;
    }

    free(wide);
    *handle = pi.hProcess;
    return PROCESS_SUCCESS;
}

bool
process_terminate(HANDLE handle) {
    return TerminateProcess(handle, 1);
}

exit_code_t
process_wait(HANDLE handle, bool close) {
    DWORD code;
    if (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0
            || !GetExitCodeProcess(handle, &code)) {
        // could not wait or retrieve the exit code
        code = NO_EXIT_CODE; // max value, it's unsigned
    }
    if (close) {
        CloseHandle(handle);
    }
    return code;
}

void
process_close(HANDLE handle) {
    bool closed = CloseHandle(handle);
    assert(closed);
    (void) closed;
}

char *
get_executable_path(void) {
    HMODULE hModule = GetModuleHandleW(NULL);
    if (!hModule) {
        return NULL;
    }
    WCHAR buf[MAX_PATH + 1]; // +1 for the null byte
    int len = GetModuleFileNameW(hModule, buf, MAX_PATH);
    if (!len) {
        return NULL;
    }
    buf[len] = '\0';
    return utf8_from_wide_char(buf);
}

bool
is_regular_file(const char *path) {
    wchar_t *wide_path = utf8_to_wide_char(path);
    if (!wide_path) {
        LOGC("Could not allocate wide char string");
        return false;
    }

    struct _stat path_stat;
    int r = _wstat(wide_path, &path_stat);
    free(wide_path);

    if (r) {
        perror("stat");
        return false;
    }
    return S_ISREG(path_stat.st_mode);
}
