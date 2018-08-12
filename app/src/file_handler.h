#ifndef FILE_HANDLER_H
#define FILE_HADNELR_H

#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_thread.h>
#include "command.h"

#define FILE_QUEUE_SIZE 16

// NOTE(AdoPi) file_queue and control_event can use a generic queue

struct file_queue {
    char *data[FILE_QUEUE_SIZE];
    int tail;
    int head;
};

struct file_handler {
    const char *serial;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    SDL_cond *event_cond;
    SDL_bool stopped;
    SDL_bool initialized;
    process_t current_process;
    struct file_queue queue;
};

SDL_bool file_handler_init(struct file_handler *file_handler, const char *serial);
void file_handler_destroy(struct file_handler *file_handler);

SDL_bool file_handler_start(struct file_handler *file_handler);
void file_handler_stop(struct file_handler *file_handler);
void file_handler_join(struct file_handler *file_handler);

SDL_bool file_handler_do(struct file_handler *file_handler, const char *filename);

#endif
