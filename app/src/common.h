#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#include "config.h"

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

// in MinGW, "%ll" is not supported as printf format for long long
# ifdef __MINGW32__
#   define PRIlld "I64d"
# else
#   define PRIlld "lld"
# endif

#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#define MIN(X,Y) (X) < (Y) ? (X) : (Y)
#define MAX(X,Y) (X) > (Y) ? (X) : (Y)

struct size {
    uint16_t width;
    uint16_t height;
};

struct point {
    int32_t x;
    int32_t y;
};

struct position {
    // The video screen size may be different from the real device screen size,
    // so store to which size the absolute position apply, to scale it
    // accordingly.
    struct size screen_size;
    struct point point;
};

#endif
