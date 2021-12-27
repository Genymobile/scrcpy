#ifndef SC_FILE_H
#define SC_FILE_H

#include "common.h"

#include <stdbool.h>

#ifdef _WIN32
# define SC_PATH_SEPARATOR '\\'
#else
# define SC_PATH_SEPARATOR '/'
#endif

#ifndef _WIN32
/**
 * Indicate if an executable exists using $PATH
 *
 * In practice, it is only used to know if a package manager is available on
 * the system. It is only implemented on Linux.
 */
bool
sc_file_executable_exists(const char *file);
#endif

/**
 * Return the absolute path of the executable (the scrcpy binary)
 *
 * The result must be freed by the caller using free(). It may return NULL on
 * error.
 */
char *
sc_file_get_executable_path(void);

/**
 * Return the absolute path of a file in the same directory as the executable
 *
 * The result must be freed by the caller using free(). It may return NULL on
 * error.
 */
char *
sc_file_get_local_path(const char *name);

/**
 * Indicate if the file exists and is not a directory
 */
bool
sc_file_is_regular(const char *path);

#endif
