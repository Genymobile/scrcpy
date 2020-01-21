#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

#include "config.h"
#include "common.h"
#include "util/net.h"

#define DEVICE_NAME_FIELD_LENGTH 64

// name must be at least DEVICE_NAME_FIELD_LENGTH bytes
bool
device_read_info(socket_t device_socket, char *device_name, struct size *size);

#endif
