#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>
#include <inttypes.h>

#ifdef _WIN32

 // not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>
# define PATH_SEPARATOR '\\'
# define PRIexitcode "lu"
// <https://stackoverflow.com/a/44383330/1987178>
# ifdef _WIN64
#   define PRIsizet PRIu64
# else
#   define PRIsizet PRIu32
# endif
# define PROCESS_NONE NULL
# define NO_EXIT_CODE -1u // max value as unsigned
  typedef HANDLE process_t;
  typedef DWORD exit_code_t;

#else

# include <sys/types.h>
# define PATH_SEPARATOR '/'
# define PRIsizet "zu"
# define PRIexitcode "d"
# define PROCESS_NONE -1
# define NO_EXIT_CODE -1
  typedef pid_t process_t;
  typedef int exit_code_t;

#endif

#include "config.h"

enum process_result {
    PROCESS_SUCCESS,
    PROCESS_ERROR_GENERIC,
    PROCESS_ERROR_MISSING_BINARY,
};

enum process_result
cmd_execute(const char *const argv[], process_t *process);

bool
cmd_terminate(process_t pid);

bool
cmd_simple_wait(process_t pid, exit_code_t *exit_code);

process_t
adb_execute(const char *serial, const char *const adb_cmd[], size_t len);

process_t
adb_forward(const char *serial, uint16_t local_port,
            const char *device_socket_name);

process_t
adb_forward_remove(const char *serial, uint16_t local_port);

process_t
adb_reverse(const char *serial, const char *device_socket_name,
            uint16_t local_port);

process_t
adb_reverse_remove(const char *serial, const char *device_socket_name);

process_t
adb_push(const char *serial, const char *local, const char *remote);

process_t
adb_install(const char *serial, const char *local);

// convenience function to wait for a successful process execution
// automatically log process errors with the provided process name
bool
process_check_success(process_t proc, const char *name);

// return the absolute path of the executable (the scrcpy binary)
// may be NULL on error; to be freed by SDL_free
char *
get_executable_path(void);

// returns true if the file exists and is not a directory
bool
is_regular_file(const char *path);

#endif
