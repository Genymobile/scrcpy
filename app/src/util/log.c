#include "log.h"

#if _WIN32
# include <windows.h>
#endif
#include <assert.h>

static SDL_LogPriority
log_level_sc_to_sdl(enum sc_log_level level) {
    switch (level) {
        case SC_LOG_LEVEL_VERBOSE:
            return SDL_LOG_PRIORITY_VERBOSE;
        case SC_LOG_LEVEL_DEBUG:
            return SDL_LOG_PRIORITY_DEBUG;
        case SC_LOG_LEVEL_INFO:
            return SDL_LOG_PRIORITY_INFO;
        case SC_LOG_LEVEL_WARN:
            return SDL_LOG_PRIORITY_WARN;
        case SC_LOG_LEVEL_ERROR:
            return SDL_LOG_PRIORITY_ERROR;
        default:
            assert(!"unexpected log level");
            return SDL_LOG_PRIORITY_INFO;
    }
}

static enum sc_log_level
log_level_sdl_to_sc(SDL_LogPriority priority) {
    switch (priority) {
        case SDL_LOG_PRIORITY_VERBOSE:
            return SC_LOG_LEVEL_VERBOSE;
        case SDL_LOG_PRIORITY_DEBUG:
            return SC_LOG_LEVEL_DEBUG;
        case SDL_LOG_PRIORITY_INFO:
            return SC_LOG_LEVEL_INFO;
        case SDL_LOG_PRIORITY_WARN:
            return SC_LOG_LEVEL_WARN;
        case SDL_LOG_PRIORITY_ERROR:
            return SC_LOG_LEVEL_ERROR;
        default:
            assert(!"unexpected log level");
            return SC_LOG_LEVEL_INFO;
    }
}

void
sc_set_log_level(enum sc_log_level level) {
    SDL_LogPriority sdl_log = log_level_sc_to_sdl(level);
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, sdl_log);
}

enum sc_log_level
sc_get_log_level(void) {
    SDL_LogPriority sdl_log = SDL_LogGetPriority(SDL_LOG_CATEGORY_APPLICATION);
    return log_level_sdl_to_sc(sdl_log);
}

void
sc_log(enum sc_log_level level, const char *fmt, ...) {
    SDL_LogPriority sdl_level = log_level_sc_to_sdl(level);

    va_list ap;
    va_start(ap, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, sdl_level, fmt, ap);
    va_end(ap);
}

#ifdef _WIN32
bool
sc_log_windows_error(const char *prefix, int error) {
    assert(prefix);

    char *message;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM;
    DWORD lang_id = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    int ret =
        FormatMessage(flags, NULL, error, lang_id, (char *) &message, 0, NULL);
    if (ret <= 0) {
        return false;
    }

    // Note: message already contains a trailing '\n'
    LOGE("%s: [%d] %s", prefix, error, message);
    LocalFree(message);
    return true;
}
#endif
