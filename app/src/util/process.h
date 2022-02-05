#ifndef SC_PROCESS_H
#define SC_PROCESS_H

#include "common.h"

#include <stdbool.h>
#include "util/thread.h"

#ifdef _WIN32

 // not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>
# define SC_PRIexitcode "lu"
# define SC_PROCESS_NONE NULL
# define SC_EXIT_CODE_NONE -1UL // max value as unsigned long
  typedef HANDLE sc_pid;
  typedef DWORD sc_exit_code;
  typedef HANDLE sc_pipe;

#else

# include <sys/types.h>
# define SC_PRIexitcode "d"
# define SC_PROCESS_NONE -1
# define SC_EXIT_CODE_NONE -1
  typedef pid_t sc_pid;
  typedef int sc_exit_code;
  typedef int sc_pipe;

#endif

struct sc_process_listener {
    void (*on_terminated)(void *userdata);
};

/**
 * Tool to observe process termination
 *
 * To keep things simple and multiplatform, it runs a separate thread to wait
 * for process termination (without closing the process to avoid race
 * conditions).
 *
 * It allows a caller to block until the process is terminated (with a
 * timeout), and to be notified asynchronously from the observer thread.
 *
 * The process is not owned by the observer (the observer will never close it).
 */
struct sc_process_observer {
    sc_pid pid;

    sc_mutex mutex;
    sc_cond cond_terminated;
    bool terminated;

    sc_thread thread;
    const struct sc_process_listener *listener;
    void *listener_userdata;
};

enum sc_process_result {
    SC_PROCESS_SUCCESS,
    SC_PROCESS_ERROR_GENERIC,
    SC_PROCESS_ERROR_MISSING_BINARY,
};

#define SC_PROCESS_NO_STDOUT (1 << 0)
#define SC_PROCESS_NO_STDERR (1 << 1)

/**
 * Execute the command and write the process id to `pid`
 *
 * The `flags` argument is a bitwise OR of the following values:
 *  - SC_PROCESS_NO_STDOUT
 *  - SC_PROCESS_NO_STDERR
 *
 * It indicates if stdout and stderr must be inherited from the scrcpy process
 * (i.e. if the process must output to the scrcpy console).
 */
enum sc_process_result
sc_process_execute(const char *const argv[], sc_pid *pid, unsigned flags);

/**
 * Execute the command and write the process id to `pid`
 *
 * If not NULL, provide a pipe for stdin (`pin`), stdout (`pout`) and stderr
 * (`perr`).
 *
 * The `flags` argument has the same semantics as in `sc_process_execute()`.
 */
enum sc_process_result
sc_process_execute_p(const char *const argv[], sc_pid *pid, unsigned flags,
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

/**
 * Start observing process
 *
 * The listener is optional. If set, its callback will be called from the
 * observer thread once the process is terminated.
 */
bool
sc_process_observer_init(struct sc_process_observer *observer, sc_pid pid,
                         const struct sc_process_listener *listener,
                         void *listener_userdata);

/**
 * Wait for process termination until a deadline
 *
 * Return true if the process is already terminated. Return false if the
 * process terminatation has not been detected yet (however, it may have
 * terminated in the meantime).
 *
 * To wait without timeout/deadline, just use sc_process_wait() instead.
 */
bool
sc_process_observer_timedwait(struct sc_process_observer *observer,
                              sc_tick deadline);

/**
 * Join the observer thread
 */
void
sc_process_observer_join(struct sc_process_observer *observer);

/**
 * Destroy the observer
 *
 * This does not close the associated process.
 */
void
sc_process_observer_destroy(struct sc_process_observer *observer);

#endif
