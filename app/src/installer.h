#ifndef APK_INSTALLER_H
#define APK_INSTALLER_H

#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>
#include "command.h"

#define APK_QUEUE_SIZE 16

// NOTE(AdoPi) apk_queue and control_event can use a generic queue

struct apk_queue {
    char *data[APK_QUEUE_SIZE];
    int tail;
    int head;
};

struct installer {
    const char *serial;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    SDL_cond *event_cond;
    SDL_bool stopped;
    SDL_bool initialized;
    process_t current_process;
    struct apk_queue queue;
};

SDL_bool installer_init(struct installer *installer, const char *serial);
void installer_destroy(struct installer *installer);

SDL_bool installer_start(struct installer *installer);
void installer_stop(struct installer *installer);
void installer_join(struct installer *installer);

// install an apk
SDL_bool installer_install_apk(struct installer *installer, const char *filename);

#endif
