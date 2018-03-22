#include "aoa.h"

#include "command.h" // must be first to include "winsock2.h" before "windows.h"
#include <libusb-1.0/libusb.h>
#include "log.h"

// <https://source.android.com/devices/accessories/aoa2>
#define AOA_GET_PROTOCOL     51
#define AOA_START_ACCESSORY  53
#define AOA_SET_AUDIO_MODE   58

#define AUDIO_MODE_NO_AUDIO               0
#define AUDIO_MODE_S16LSB_STEREO_44100HZ  1

#define DEFAULT_TIMEOUT 1000

typedef struct control_params {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    unsigned char *data;
    uint16_t length;
    unsigned int timeout;
} control_params;

static void log_libusb_error(enum libusb_error errcode) {
    LOGE("%s", libusb_strerror(errcode));
}

static SDL_bool control_transfer(libusb_device_handle *handle, control_params *params) {
    int r = libusb_control_transfer(handle,
                                    params->request_type,
                                    params->request,
                                    params->value,
                                    params->index,
                                    params->data,
                                    params->length,
                                    params->timeout);
    if (r < 0) {
        log_libusb_error(r);
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

static SDL_bool get_serial(libusb_device *device, struct libusb_device_descriptor *desc, unsigned char *data, int length) {

    libusb_device_handle *handle;
    int r;
    if ((r = libusb_open(device, &handle))) {
        // silently ignore
        LOGD("USB: cannot open device %04x:%04x (%s)", desc->idVendor, desc->idProduct, libusb_strerror(r));
        return SDL_FALSE;
    }

    if (!desc->iSerialNumber) {
        LOGD("USB: device %04x:%04x has no serial number available", desc->idVendor, desc->idProduct);
        libusb_close(handle);
        return SDL_FALSE;
    }

    if ((r = libusb_get_string_descriptor_ascii(handle, desc->iSerialNumber, data, length)) <= 0) {
        // silently ignore
        LOGD("USB: cannot read serial of device %04x:%04x (%s)", desc->idVendor, desc->idProduct, libusb_strerror(r));
        libusb_close(handle);
        return SDL_FALSE;
    }
    data[length - 1] = '\0'; // just in case

    libusb_close(handle);
    return SDL_TRUE;
}

static libusb_device *find_device(const char *serial) {
    libusb_device **list;
    libusb_device *found = NULL;
    ssize_t cnt = libusb_get_device_list(NULL, &list);
    ssize_t i = 0;
    if (cnt < 0) {
        log_libusb_error(cnt);
        return NULL;
    }
    for (i = 0; i < cnt; ++i) {
        libusb_device *device = list[i];

        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(device, &desc);

        char usb_serial[128];
        if (get_serial(device, &desc, (unsigned char *) usb_serial, sizeof(usb_serial))) {
            if (!strncmp(serial, usb_serial, sizeof(usb_serial))) {
                libusb_ref_device(device);
                found = device;
                LOGD("USB device with serial %s found: %04x:%04x", serial, desc.idVendor, desc.idProduct);
                break;
            }
        }
    }
    libusb_free_device_list(list, 1);
    return found;
}

static SDL_bool aoa_get_protocol(libusb_device_handle *handle, uint16_t *version) {
    unsigned char data[2];
    control_params params = {
        .request_type = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR,
        .request = AOA_GET_PROTOCOL,
        .value = 0,
        .index = 0,
        .data = data,
        .length = sizeof(data),
        .timeout = DEFAULT_TIMEOUT
    };
    if (control_transfer(handle, &params)) {
        // little endian
        *version = (data[1] << 8) | data[0];
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

static SDL_bool set_audio_mode(libusb_device_handle *handle, uint16_t mode) {
    control_params params = {
        .request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
        .request = AOA_SET_AUDIO_MODE,
        // <https://source.android.com/devices/accessories/aoa2.html#audio-support>
        .value = mode,
        .index = 0, // unused
        .data = NULL,
        .length = 0,
        .timeout = DEFAULT_TIMEOUT
    };
    return control_transfer(handle, &params);
}

static SDL_bool start_accessory(libusb_device_handle *handle) {
    control_params params = {
        .request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR,
        .request = AOA_START_ACCESSORY,
        .value = 0, // unused
        .index = 0, // unused
        .data = NULL,
        .length = 0,
        .timeout = DEFAULT_TIMEOUT
    };
    return control_transfer(handle, &params);
}

SDL_bool aoa_init(void) {
    return !libusb_init(NULL);
}

void aoa_exit(void) {
    libusb_exit(NULL);
}

SDL_bool aoa_forward_audio(const char *serial, SDL_bool forward) {
    LOGD("%s audio accessory...", forward ? "Enabling" : "Disabling");
    libusb_device *device = find_device(serial);
    if (!device) {
        LOGE("Cannot find USB device having serial %s", serial);
        return SDL_FALSE;
    }

    SDL_bool ret = SDL_FALSE;

    libusb_device_handle *handle;
    int r = libusb_open(device, &handle);
    if (r) {
        log_libusb_error(r);
        goto finally_unref_device;
    }

    uint16_t version;
    if (!aoa_get_protocol(handle, &version)) {
        LOGE("Cannot get AOA protocol version");
        goto finally_close_handle;
    }

    LOGD("Device AOA version: %" PRIu16 "\n", version);
    if (version < 2) {
        LOGE("Device does not support AOA 2: %" PRIu16, version);
        goto finally_close_handle;
    }

    uint16_t mode = forward ? AUDIO_MODE_S16LSB_STEREO_44100HZ : AUDIO_MODE_NO_AUDIO;
    if (!set_audio_mode(handle, mode)) {
        LOGE("Cannot set audio mode: %" PRIu16, mode);
        goto finally_close_handle;
    }

    if (!start_accessory(handle)) {
        LOGE("Cannot start accessory");
        return SDL_FALSE;
    }

    ret = SDL_TRUE;

finally_close_handle:
    libusb_close(handle);
finally_unref_device:
    libusb_unref_device(device);

    return ret;
}
