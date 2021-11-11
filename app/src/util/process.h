#ifndef SC_PROCESS_H
#define SC_PROCESS_H

#include "common.h"

#include <stdbool.h>

#ifdef _WIN32

 // not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>
# define SC_PRIexitcode "lu"
// <https://stackoverflow.com/a/44383330/1987178>
# define SC_PRIsizet "Iu"
# define SC_PROCESS_NONE NULL
# define SC_EXIT_CODE_NONE -1u // max value as unsigned
  typedef HANDLE sc_pid;
  typedef DWORD sc_exit_code;
  typedef HANDLE sc_pipe;

#else

# include <sys/types.h>
# define SC_PRIsizet "zu"
# define SC_PRIexitcode "d"
# define SC_PROCESS_NONE -1
# define SC_EXIT_CODE_NONE -1
  typedef pid_t sc_pid;
  typedef int sc_exit_code;
  typedef int sc_pipe;

#endif

enum sc_process_result {
    SC_PROCESS_SUCCESS,
    SC_PROCESS_ERROR_GENERIC,
    SC_PROCESS_ERROR_MISSING_BINARY,
};

/**
 * Execute the command and write the process id to `pid`
 */
enum sc_process_result
sc_process_execute(const char *const argv[], sc_pid *pid);

/**
 * Execute the command and write the process id to `pid`
 *
 * If not NULL, provide a pipe for stdin (`pin`), stdout (`pout`) and stderr
 * (`perr`).
 */
enum sc_process_result
sc_process_execute_p(const char *const argv[], sc_pid *pid,
                     sc_pipe *pin, sc_pipe *pout, sc_pipe *perr);

/**
 * Kill the process
 */
bool
sc_process_terminate(sc_pid pid);

/**
 * Wait and close the process (similar to waitpid())
 *
 * The `close` flag indicates if the process must be _closed_ (reaped) (passing
 * false is equivalent to enable WNOWAIT in waitid()).
 */
sc_exit_code
sc_process_wait(sc_pid pid, bool close);

/**
 * Close (reap) the process
 *
 * Semantically:
 *    sc_process_wait(close) = sc_process_wait(noclose) + sc_process_close()
 */
void
sc_process_close(sc_pid pid);

/**
 * Convenience function to wait for a successful process execution
 *
 * Automatically log process errors with the provided process name.
 */
bool
sc_process_check_success(sc_pid pid, const char *name, bool close);

/**
 * Read from the pipe
 *
 * Same semantic as read().
 */
ssize_t
sc_pipe_read(sc_pipe pipe, char *data, size_t len);

/**
 * Read exactly `len` chars from a pipe (unless EOF)
 */
ssize_t
sc_pipe_read_all(sc_pipe pipe, char *data, size_t len);

/**
 * Close the pipe
 */
void
sc_pipe_close(sc_pipe pipe);

#endif
