#include "device.h"
#include "log.h"

bool
device_read_info(socket_t device_socket, char *device_name, char *device_serial, struct size *size) {
    unsigned char buf[DEVICE_INFO_FIELD_LENGTH*DEVICE_INFO_FIELD_COUNT + 4];
    int r = net_recv_all(device_socket, buf, sizeof(buf));
    if (r < DEVICE_INFO_FIELD_LENGTH*DEVICE_INFO_FIELD_COUNT + 4) {
        LOGE("Could not retrieve device information");
        return false;
    }
    // in case the client sends garbage
    for (int i=1; i<=DEVICE_INFO_FIELD_COUNT; i++)
        buf[DEVICE_INFO_FIELD_LENGTH*i - 1] = '\0';
    // strcpy is safe here, since name/serial contains at least
    // DEVICE_INFO_FIELD_LENGTH bytes and strlen(buf) < DEVICE_INFO_FIELD_LENGTH
    strcpy(device_name, (char *) buf);
    strcpy(device_serial, (char *) (buf+DEVICE_INFO_FIELD_LENGTH) );
    size->width = (buf[DEVICE_INFO_FIELD_LENGTH*DEVICE_INFO_FIELD_COUNT] << 8)
            | buf[DEVICE_INFO_FIELD_LENGTH*DEVICE_INFO_FIELD_COUNT + 1];
    size->height = (buf[DEVICE_INFO_FIELD_LENGTH*DEVICE_INFO_FIELD_COUNT + 2] << 8)
            | buf[DEVICE_INFO_FIELD_LENGTH*DEVICE_INFO_FIELD_COUNT + 3];
    return true;
}
