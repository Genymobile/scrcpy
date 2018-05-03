#ifndef APK_INSTALLER_H
#define APK_INSTALLER_H

#define APK_QUEUE_SIZE 16
#define MAX_FILENAME_SIZE 1024


#include "apkinstaller.h"

#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>

// NOTE(AdoPi) apk_queue and control_event can use a generic queue

struct apk_queue {
    char data[APK_QUEUE_SIZE][MAX_FILENAME_SIZE];
    int tail;
    int head;
};

struct installer {
    const char* serial;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    SDL_cond *event_cond;
    SDL_bool stopped;
    SDL_bool initialized;
    struct apk_queue queue;
};

#define INSTALLER_INITIALIZER { \
    .serial = NULL,             \
    .thread = NULL,             \
    .mutex = NULL,              \
    .event_cond = NULL,         \
    .stopped = SDL_FALSE,       \
    .initialized = SDL_FALSE    \
}


SDL_bool installer_init(struct installer *installer, const char* serial);
void installer_destroy(struct installer *installer);

SDL_bool installer_start(struct installer *installer);
void installer_stop(struct installer *installer);
void installer_join(struct installer *installer);

// install an apk
SDL_bool installer_push_apk(struct installer *installer, const char* filename);

#endif
