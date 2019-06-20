#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

#include "common.h"
#include "net.h"

#define DEVICE_INFO_FIELD_LENGTH 64
#define DEVICE_INFO_FIELD_COUNT 2
#define DEVICE_SDCARD_PATH "/sdcard/"

// name and serial must be at least DEVICE_INFO_FIELD_LENGTH bytes
bool
device_read_info(socket_t device_socket, char *device_name, char *device_serial, struct size *size);

#endif
