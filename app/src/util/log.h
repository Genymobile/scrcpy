#ifndef LOG_H
#define LOG_H

#include <SDL2/SDL_log.h>

enum sc_log_level {
    SC_LOG_LEVEL_DEBUG,
    SC_LOG_LEVEL_INFO,
    SC_LOG_LEVEL_WARN,
    SC_LOG_LEVEL_ERROR,
};

#define LOGV(...) SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOGD(...) SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOGI(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOGW(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOGE(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOGC(...) SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

#endif
