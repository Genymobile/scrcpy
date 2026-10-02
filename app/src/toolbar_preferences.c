#include "toolbar_preferences.h"

#include <stdlib.h>
#include <SDL3/SDL.h>

#include "util/log.h"

#define SC_TOOLBAR_VISIBILITY_FILENAME "toolbar-visible.txt"

static char *
sc_toolbar_preferences_get_path(void) {
    char *pref = SDL_GetPrefPath("Genymobile", "scrcpy");
    if (!pref) {
        LOGW("Could not resolve toolbar preferences directory: %s",
             SDL_GetError());
        return NULL;
    }

    char *path = NULL;
    if (asprintf(&path, "%s%s", pref,
                 SC_TOOLBAR_VISIBILITY_FILENAME) == -1) {
        LOG_OOM();
        path = NULL;
    }
    SDL_free(pref);
    return path;
}

bool
sc_toolbar_preferences_load_visible(void) {
    char *path = sc_toolbar_preferences_get_path();
    if (!path) {
        return false;
    }

    size_t size = 0;
    char *data = SDL_LoadFile(path, &size);
    free(path);
    if (!data) {
        // Missing preferences are expected on the first run.
        return false;
    }

    bool visible = size == 1 && data[0] == '1';
    if (size != 1 || (data[0] != '0' && data[0] != '1')) {
        LOGW("Ignoring invalid toolbar visibility preference");
    }
    SDL_free(data);
    return visible;
}

bool
sc_toolbar_preferences_save_visible(bool visible) {
    char *path = sc_toolbar_preferences_get_path();
    if (!path) {
        return false;
    }

    char value = visible ? '1' : '0';
    bool ok = SDL_SaveFile(path, &value, sizeof(value));
    if (!ok) {
        LOGW("Could not save toolbar visibility preference: %s",
             SDL_GetError());
    }
    free(path);
    return ok;
}
