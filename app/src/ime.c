#include "ime.h"

#include <stdlib.h>
#include <string.h>

#include "adb/adb.h"
#include "util/env.h"
#include "util/file.h"
#include "util/log.h"

#define SC_IME_FILENAME "scrcpy-ime.apk"
#define SC_IME_PATH_DEFAULT PREFIX "/share/scrcpy/" SC_IME_FILENAME

static char *
get_ime_path(bool *overridden) {
    char *ime_path = sc_get_env("SCRCPY_IME_PATH");
    if (ime_path) {
        LOGD("Using SCRCPY_IME_PATH: %s", ime_path);
        *overridden = true;
        return ime_path;
    }

    *overridden = false;
#ifndef PORTABLE
    ime_path = strdup(SC_IME_PATH_DEFAULT);
#else
    ime_path = sc_file_get_local_path(SC_IME_FILENAME);
#endif
    return ime_path;
}

static bool
install_ime(struct sc_intr *intr, const char *serial, const char *ime_path) {
    LOGI("Installing scrcpy IME...");
    enum sc_adb_install_result result =
        sc_adb_install_detailed(intr, serial, ime_path);
    if (result == SC_ADB_INSTALL_RESULT_SUCCESS) {
        return true;
    }
    if (result != SC_ADB_INSTALL_RESULT_SIGNATURE_MISMATCH) {
        LOGE("Could not install scrcpy IME");
        return false;
    }

    LOGW("The installed scrcpy IME has a different signature; uninstalling "
         "the old package");
    if (!sc_adb_uninstall(intr, serial, SC_IME_PACKAGE_NAME)) {
        LOGE("Could not uninstall the conflicting scrcpy IME package");
        return false;
    }

    LOGI("Installing scrcpy IME after removing the conflicting package...");
    result = sc_adb_install_detailed(intr, serial, ime_path);
    if (result != SC_ADB_INSTALL_RESULT_SUCCESS) {
        LOGE("The old scrcpy IME was uninstalled, but the replacement could "
             "not be installed");
        return false;
    }
    return true;
}

bool
sc_ime_prepare_device(struct sc_intr *intr, const char *serial) {
    char *installed_version;
    if (!sc_adb_get_package_version(intr, serial, SC_IME_PACKAGE_NAME,
                                    &installed_version)) {
        LOGE("Could not query the installed scrcpy IME version");
        return false;
    }

    bool overridden;
    char *ime_path = get_ime_path(&overridden);
    if (!ime_path) {
        free(installed_version);
        return false;
    }

    bool install = overridden || !installed_version
        || strcmp(installed_version, SCRCPY_VERSION);
    if (!install) {
        LOGD("scrcpy IME %s is already installed", installed_version);
        free(installed_version);
        free(ime_path);
        return true;
    }

    if (installed_version) {
        LOGI("Updating scrcpy IME from %s to %s", installed_version,
             SCRCPY_VERSION);
    }
    free(installed_version);

    if (!sc_file_is_regular(ime_path)) {
        LOGE("scrcpy IME APK not found: %s", ime_path);
        free(ime_path);
        return false;
    }

    bool ok = install_ime(intr, serial, ime_path);
    free(ime_path);
    if (!ok) {
        return false;
    }

    char *verified_version;
    if (!sc_adb_get_package_version(intr, serial, SC_IME_PACKAGE_NAME,
                                    &verified_version)) {
        LOGE("Could not verify the installed scrcpy IME version");
        return false;
    }
    ok = verified_version && !strcmp(verified_version, SCRCPY_VERSION);
    if (!ok) {
        LOGE("Unexpected scrcpy IME version after installation (expected %s, "
             "got %s)", SCRCPY_VERSION,
             verified_version ? verified_version : "none");
    }
    free(verified_version);
    return ok;
}
