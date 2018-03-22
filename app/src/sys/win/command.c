#include "command.h"

#include "log.h"
#include "strutil.h"

static int build_cmd(char *cmd, size_t len, const char *const argv[]) {
    // Windows command-line parsing is WTF:
    // <http://daviddeley.com/autohotkey/parameters/parameters.htm#WINPASS>
    // only make it work for this very specific program
    // (don't handle escaping nor quotes)
    size_t ret = xstrjoin(cmd, argv, ' ', len);
    if (ret >= len) {
        LOGE("Command too long (%" PRIsizet " chars)", len - 1);
        return -1;
    }
    return 0;
}

HANDLE cmd_execute(const char *path, const char *const argv[]) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    char cmd[256];
    if (build_cmd(cmd, sizeof(cmd), argv)) {
        return NULL;
    }

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return NULL;
    }

    return pi.hProcess;
}

HANDLE cmd_execute_redirect(const char *path, const char *const argv[],
                            HANDLE *pipe_stdin, HANDLE *pipe_stdout, HANDLE *pipe_stderr) {
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
             return NULL;
        }
    }
    if (pipe_stdout) {
        if (!CreatePipe(pipe_stdout, &stdout_write_handle, &sa, 0)) {
             perror("pipe");
             // clean up
             if (pipe_stdin) {
                CloseHandle(&stdin_read_handle);
                CloseHandle(pipe_stdin);
             }
             return NULL;
        }
    }
    if (pipe_stderr) {
        if (!CreatePipe(pipe_stderr, &stderr_write_handle, &sa, 0)) {
            perror("pipe");
            // clean up
             if (pipe_stdin) {
                CloseHandle(&stdin_read_handle);
                CloseHandle(pipe_stdin);
             }
            if (pipe_stdout) {
                CloseHandle(pipe_stdout);
                CloseHandle(&stdout_write_handle);
            }
        }
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
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

    char cmd[256];
    if (build_cmd(cmd, sizeof(cmd), argv)) {
        return NULL;
    }

    if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return NULL;
    }

    return pi.hProcess;
}

SDL_bool cmd_terminate(HANDLE handle) {
    return TerminateProcess(handle, 1) && CloseHandle(handle);
}

SDL_bool cmd_simple_wait(HANDLE handle, DWORD *exit_code) {
    DWORD code;
    if (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0 || !GetExitCodeProcess(handle, &code)) {
        // cannot wait or retrieve the exit code
        code = -1; // max value, it's unsigned
    }
    if (exit_code) {
        *exit_code = code;
    }
    return !code;
}

int read_pipe(HANDLE pipe, char *data, size_t len) {
    DWORD r;
    if (!ReadFile(pipe, data, len, &r, NULL)) {
        return -1;
    }
    return r;
}

void close_pipe(HANDLE pipe) {
    if (!CloseHandle(pipe)) {
        LOGW("Cannot close pipe");
    }
}
