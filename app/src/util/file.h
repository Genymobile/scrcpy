#ifndef SC_FILE_H
#define SC_FILE_H

#include "common.h"

#include <stdbool.h>

#ifdef _WIN32
# define PATH_SEPARATOR '\\'
#else
# define PATH_SEPARATOR '/'
#endif

#ifndef _WIN32
// only used to find package manager, not implemented for Windows
bool
search_executable(const char *file);
#endif

// return the absolute path of the executable (the scrcpy binary)
// may be NULL on error; to be freed by free()
char *
get_executable_path(void);

// Return the absolute path of a file in the same directory as he executable.
// May be NULL on error. To be freed by free().
char *
get_local_file_path(const char *name);

// returns true if the file exists and is not a directory
bool
is_regular_file(const char *path);

#endif
