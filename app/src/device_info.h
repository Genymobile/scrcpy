#ifndef SC_DEVICE_INFO_H
#define SC_DEVICE_INFO_H

#include "common.h"

#include "util/intr.h"

struct sc_device_info {
    char *android_version;
    char *api_level;
};

void
sc_device_info_init(struct sc_device_info *info);

void
sc_device_info_load(struct sc_device_info *info, struct sc_intr *intr,
                    const char *serial);

void
sc_device_info_destroy(struct sc_device_info *info);

char *
sc_device_info_build_window_title(const struct sc_device_info *info,
                                  const char *base_title);

char *
sc_device_info_build_subtitle(const struct sc_device_info *info);

#endif
