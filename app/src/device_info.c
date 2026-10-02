#include "device_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adb/adb.h"

void
sc_device_info_init(struct sc_device_info *info) {
    info->android_version = NULL;
    info->api_level = NULL;
}

void
sc_device_info_load(struct sc_device_info *info, struct sc_intr *intr,
                    const char *serial) {
    free(info->android_version);
    free(info->api_level);
    info->android_version =
        sc_adb_getprop(intr, serial, "ro.build.version.release",
                       SC_ADB_SILENT);
    info->api_level =
        sc_adb_getprop(intr, serial, "ro.build.version.sdk", SC_ADB_SILENT);
}

void
sc_device_info_destroy(struct sc_device_info *info) {
    free(info->android_version);
    free(info->api_level);
}

char *
sc_device_info_build_window_title(const struct sc_device_info *info,
                                  const char *base_title) {
    if (!info->android_version || !*info->android_version) {
        return strdup(base_title);
    }

    char *title;
    int r;
    if (info->api_level && *info->api_level) {
        r = asprintf(&title, "%s · Android %s | API %s", base_title,
                     info->android_version, info->api_level);
    } else {
        r = asprintf(&title, "%s · Android %s", base_title,
                     info->android_version);
    }
    return r == -1 ? NULL : title;
}

char *
sc_device_info_build_subtitle(const struct sc_device_info *info) {
    if (!info->android_version || !*info->android_version) {
        return NULL;
    }

    char *subtitle;
    int r;
    if (info->api_level && *info->api_level) {
        r = asprintf(&subtitle, "Android %s | API %s", info->android_version,
                     info->api_level);
    } else {
        r = asprintf(&subtitle, "Android %s", info->android_version);
    }
    return r == -1 ? NULL : subtitle;
}
