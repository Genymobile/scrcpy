#include "log.h"

#if _WIN32
# include <windows.h>
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <libavutil/log.h>

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
    SDL_LogSetPriority(SDL_LOG_CATEGORY_CUSTOM, sdl_log);
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

static SDL_LogPriority
sdl_priority_from_av_level(int level) {
    switch (level) {
        case AV_LOG_PANIC:
        case AV_LOG_FATAL:
            return SDL_LOG_PRIORITY_CRITICAL;
        case AV_LOG_ERROR:
            return SDL_LOG_PRIORITY_ERROR;
        case AV_LOG_WARNING:
            return SDL_LOG_PRIORITY_WARN;
        case AV_LOG_INFO:
            return SDL_LOG_PRIORITY_INFO;
    }
    // do not forward others, which are too verbose
    return 0;
}

static void
sc_av_log_callback(void *avcl, int level, const char *fmt, va_list vl) {
    (void) avcl;
    SDL_LogPriority priority = sdl_priority_from_av_level(level);
    if (priority == 0) {
        return;
    }

    size_t fmt_len = strlen(fmt);
    char *local_fmt = malloc(fmt_len + 10);
    if (!local_fmt) {
        LOG_OOM();
        return;
    }
    memcpy(local_fmt, "[FFmpeg] ", 9); // do not write the final '\0'
    memcpy(local_fmt + 9, fmt, fmt_len + 1); // include '\0'
    SDL_LogMessageV(SDL_LOG_CATEGORY_CUSTOM, priority, local_fmt, vl);
    free(local_fmt);
}

static const char *const sc_sdl_log_priority_names[SDL_NUM_LOG_PRIORITIES] = {
    [SDL_LOG_PRIORITY_VERBOSE] = "VERBOSE",
    [SDL_LOG_PRIORITY_DEBUG] = "DEBUG",
    [SDL_LOG_PRIORITY_INFO] = "INFO",
    [SDL_LOG_PRIORITY_WARN] = "WARN",
    [SDL_LOG_PRIORITY_ERROR] = "ERROR",
    [SDL_LOG_PRIORITY_CRITICAL] = "CRITICAL",
};

static void SDLCALL
sc_sdl_log_print(void *userdata, int category, SDL_LogPriority priority,
                 const char *message) {
    (void) userdata;
    (void) category;

    FILE *out = priority < SDL_LOG_PRIORITY_WARN ? stdout : stderr;
    assert(priority < SDL_NUM_LOG_PRIORITIES);
    const char *prio_name = sc_sdl_log_priority_names[priority];
    fprintf(out, "%s: %s\n", prio_name, message);
}

void
sc_log_configure(void) {
    SDL_LogSetOutputFunction(sc_sdl_log_print, NULL);
    // Redirect FFmpeg logs to SDL logs
    av_log_set_callback(sc_av_log_callback);
}
