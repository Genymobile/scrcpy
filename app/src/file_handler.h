#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <stdbool.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>

#include "command.h"

#define REQUEST_QUEUE_SIZE 16

typedef enum {
    ACTION_INSTALL_APK,
    ACTION_PUSH_FILE,
} file_handler_action_t;

struct request_queue {
    struct request *reqs[REQUEST_QUEUE_SIZE];
    int tail;
    int head;
};

struct file_handler {
    const char *serial;
    SDL_Thread *thread;
    SDL_mutex *mutex;
    SDL_cond *event_cond;
    bool stopped;
    bool initialized;
    process_t current_process;
    struct request_queue queue;
};

bool
file_handler_init(struct file_handler *file_handler, const char *serial);

void
file_handler_destroy(struct file_handler *file_handler);

bool
file_handler_start(struct file_handler *file_handler);

void
file_handler_stop(struct file_handler *file_handler);

void
file_handler_join(struct file_handler *file_handler);

bool
file_handler_request(struct file_handler *file_handler,
                     file_handler_action_t action,
                     const char *file);

#endif
