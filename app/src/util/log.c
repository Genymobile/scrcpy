#include "log.h"

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
