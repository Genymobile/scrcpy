#include "util/process.h"

#include <assert.h>

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
process_execute_redirect(const char *const argv[], HANDLE *handle,
                         HANDLE *pipe_stdin, HANDLE *pipe_stdout,
                         HANDLE *pipe_stderr) {
    enum process_result ret = PROCESS_ERROR_GENERIC;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE stdin_read_handle;
    HANDLE stdout_write_handle;
    HANDLE stderr_write_handle;
    if (pipe_stdin) {
        if (!CreatePipe(&stdin_read_handle, pipe_stdin, &sa, 0)) {
            perror("pipe");
            return PROCESS_ERROR_GENERIC;
        }
        if (!SetHandleInformation(*pipe_stdin, HANDLE_FLAG_INHERIT, 0)) {
            LOGE("SetHandleInformation stdin failed");
            goto error_close_stdin;
        }
    }
    if (pipe_stdout) {
        if (!CreatePipe(pipe_stdout, &stdout_write_handle, &sa, 0)) {
            perror("pipe");
            goto error_close_stdin;
        }
        if (!SetHandleInformation(*pipe_stdout, HANDLE_FLAG_INHERIT, 0)) {
            LOGE("SetHandleInformation stdout failed");
            goto error_close_stdout;
        }
    }
    if (pipe_stderr) {
        if (!CreatePipe(pipe_stderr, &stderr_write_handle, &sa, 0)) {
            perror("pipe");
            goto error_close_stdout;
        }
        if (!SetHandleInformation(*pipe_stderr, HANDLE_FLAG_INHERIT, 0)) {
            LOGE("SetHandleInformation stderr failed");
            goto error_close_stderr;
        }
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (pipe_stdin || pipe_stdout || pipe_stderr) {
        si.dwFlags = STARTF_USESTDHANDLES;
        if (pipe_stdin) {
            si.hStdInput = stdin_read_handle;
        }
        if (pipe_stdout) {
            si.hStdOutput = stdout_write_handle;
        }
        if (pipe_stderr) {
            si.hStdError = stderr_write_handle;
        }
    }

    char *cmd = malloc(CMD_MAX_LEN);
    if (!cmd || !build_cmd(cmd, CMD_MAX_LEN, argv)) {
        *handle = NULL;
        goto error_close_stderr;
    }

    wchar_t *wide = utf8_to_wide_char(cmd);
    free(cmd);
    if (!wide) {
        LOGC("Could not allocate wide char string");
        goto error_close_stderr;
    }

    if (!CreateProcessW(NULL, wide, NULL, NULL, TRUE, 0, NULL, NULL, &si,
                        &pi)) {
        free(wide);
        *handle = NULL;

        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            ret = PROCESS_ERROR_MISSING_BINARY;
        }
        goto error_close_stderr;
    }

    // These handles are used by the child process, close them for this process
    if (pipe_stdin) {
        CloseHandle(stdin_read_handle);
    }
    if (pipe_stdout) {
        CloseHandle(stdout_write_handle);
    }
    if (pipe_stderr) {
        CloseHandle(stderr_write_handle);
    }

    free(wide);
    *handle = pi.hProcess;

    return PROCESS_SUCCESS;

error_close_stderr:
    if (pipe_stderr) {
        CloseHandle(*pipe_stderr);
        CloseHandle(stderr_write_handle);
    }
error_close_stdout:
    if (pipe_stdout) {
        CloseHandle(*pipe_stdout);
        CloseHandle(stdout_write_handle);
    }
error_close_stdin:
    if (pipe_stdin) {
        CloseHandle(*pipe_stdin);
        CloseHandle(stdin_read_handle);
    }

    return ret;
}

enum process_result
process_execute(const char *const argv[], HANDLE *handle) {
    return process_execute_redirect(argv, handle, NULL, NULL, NULL);
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

ssize_t
read_pipe(HANDLE pipe, char *data, size_t len) {
    DWORD r;
    if (!ReadFile(pipe, data, len, &r, NULL)) {
        return -1;
    }
    return r;
}

void
close_pipe(HANDLE pipe) {
    if (!CloseHandle(pipe)) {
        LOGW("Cannot close pipe");
    }
}
