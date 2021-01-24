#ifndef SC_PROCESS_H
#define SC_PROCESS_H

#include "common.h"

#include <stdbool.h>

#ifdef _WIN32

 // not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>
# define PATH_SEPARATOR '\\'
# define PRIexitcode "lu"
// <https://stackoverflow.com/a/44383330/1987178>
# define PRIsizet "Iu"
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

enum process_result {
    PROCESS_SUCCESS,
    PROCESS_ERROR_GENERIC,
    PROCESS_ERROR_MISSING_BINARY,
};

// execute the command and write the result to the output parameter "process"
enum process_result
process_execute(const char *const argv[], process_t *process);

// kill the process
bool
process_terminate(process_t pid);

// wait and close the process (like waitpid())
// the "close" flag indicates if the process must be "closed" (reaped)
// (passing false is equivalent to enable WNOWAIT in waitid())
exit_code_t
process_wait(process_t pid, bool close);

// close the process
//
// Semantically, process_wait(close) = process_wait(noclose) + process_close
void
process_close(process_t pid);

// convenience function to wait for a successful process execution
// automatically log process errors with the provided process name
bool
process_check_success(process_t proc, const char *name, bool close);

#ifndef _WIN32
// only used to find package manager, not implemented for Windows
bool
search_executable(const char *file);
#endif

// return the absolute path of the executable (the scrcpy binary)
// may be NULL on error; to be freed by free()
char *
get_executable_path(void);

// returns true if the file exists and is not a directory
bool
is_regular_file(const char *path);

#endif
