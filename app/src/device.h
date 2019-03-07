#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

#include "common.h"
#include "net.h"

#define DEVICE_NAME_FIELD_LENGTH 64
#define DEVICE_SDCARD_PATH "/sdcard/"

// name must be at least DEVICE_NAME_FIELD_LENGTH bytes
bool
device_read_info(socket_t device_socket, char *name, struct size *frame_size);

#endif
