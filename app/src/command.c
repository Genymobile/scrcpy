#include "command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "log.h"

static const char *adb_command;

static inline const char *get_adb_command() {
    if (!adb_command) {
        adb_command = getenv("ADB");
        if (!adb_command)
            adb_command = "adb";
    }
    return adb_command;
}

static void fill_cmd(const char *cmd[], const char *serial, const char *const adb_cmd[], int len) {
    int i;
    cmd[0] = get_adb_command();
    if (serial) {
        cmd[1] = "-s";
        cmd[2] = serial;
        i = 3;
    } else {
        i = 1;
    }

    memcpy(&cmd[i], adb_cmd, len * sizeof(const char *));
    cmd[len + i] = NULL;
}

process_t adb_execute(const char *serial, const char *const adb_cmd[], int len) {
    const char *cmd[len + 4];
    fill_cmd(cmd, serial, adb_cmd, len);
    return cmd_execute(cmd[0], cmd);
}

process_t adb_execute_redirect(const char *serial, const char *const adb_cmd[], int len,
                               pipe_t *pipe_stdin, pipe_t *pipe_stdout, pipe_t *pipe_stderr) {
    const char *cmd[len + 4];
    fill_cmd(cmd, serial, adb_cmd, len);
    return cmd_execute_redirect(cmd[0], cmd, pipe_stdin, pipe_stdout, pipe_stderr);
}

process_t adb_forward(const char *serial, uint16_t local_port, const char *device_socket_name) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"forward", local, remote};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

process_t adb_forward_remove(const char *serial, uint16_t local_port) {
    char local[4 + 5 + 1]; // tcp:PORT
    sprintf(local, "tcp:%" PRIu16, local_port);
    const char *const adb_cmd[] = {"forward", "--remove", local};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

process_t adb_reverse(const char *serial, const char *device_socket_name, uint16_t local_port) {
    char local[4 + 5 + 1]; // tcp:PORT
    char remote[108 + 14 + 1]; // localabstract:NAME
    sprintf(local, "tcp:%" PRIu16, local_port);
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", remote, local};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

process_t adb_reverse_remove(const char *serial, const char *device_socket_name) {
    char remote[108 + 14 + 1]; // localabstract:NAME
    snprintf(remote, sizeof(remote), "localabstract:%s", device_socket_name);
    const char *const adb_cmd[] = {"reverse", "--remove", remote};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

process_t adb_push(const char *serial, const char *local, const char *remote) {
    const char *const adb_cmd[] = {"push", local, remote};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

process_t adb_remove_path(const char *serial, const char *path) {
    const char *const adb_cmd[] = {"shell", "rm", path};
    return adb_execute(serial, adb_cmd, ARRAY_LEN(adb_cmd));
}

static int adb_execute_get_output(const char *serial, const char *const adb_cmd[], int adb_cmd_len,
                           char *data, size_t data_len, const char *name) {
    pipe_t pipe_stdout;
    process_t proc = adb_execute_redirect(serial, adb_cmd, adb_cmd_len, NULL, &pipe_stdout, NULL);
    if (!process_check_success(proc, name)) {
        return -1;
    }
    int r = read_pipe(pipe_stdout, data, data_len);
    close_pipe(pipe_stdout);
    return r;
}

static int truncate_first_line(char *data, int len) {
    data[len - 1] = '\0';
    char *eol = strpbrk(data, "\r\n");
    if (eol) {
        *eol = '\0';
        len = eol - data;
    }
    return len;
}

int adb_read_serialno(const char *serial, char *data, size_t len) {
    const char *const adb_cmd[] = {"get-serialno"};
    int r = adb_execute_get_output(serial, adb_cmd, ARRAY_LEN(adb_cmd), data, len, "get-serialno");
    return r <= 0 ? r : truncate_first_line(data, r);
}

int adb_read_model(const char *serial, char *data, size_t len) {
    const char *const adb_cmd[] = {"shell", "getprop", "ro.product.model"};
    int r = adb_execute_get_output(serial, adb_cmd, ARRAY_LEN(adb_cmd), data, len, "getprop model");
    return r <= 0 ? r : truncate_first_line(data, r);
}

SDL_bool process_check_success(process_t proc, const char *name) {
    if (proc == PROCESS_NONE) {
        LOGE("Could not execute \"%s\"", name);
        return SDL_FALSE;
    }
    exit_code_t exit_code;
    if (!cmd_simple_wait(proc, &exit_code)) {
        if (exit_code != NO_EXIT_CODE) {
            LOGE("\"%s\" returned with value %" PRIexitcode, name, exit_code);
        } else {
            LOGE("\"%s\" exited unexpectedly", name);
        }
        return SDL_FALSE;
    }
    return SDL_TRUE;
}
