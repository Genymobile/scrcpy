#include "mapping_manager.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL3/SDL.h>

#include "util/cJSON.h"
#include "util/log.h"

// ============================================================
// Path helpers
// ============================================================

char *
sc_mapping_get_base_dir(void) {
    const char *base = SDL_GetBasePath();
    if (base) {
        char *copy = SDL_strdup(base);
        SDL_free((void *)base);
        return copy;
    }
    // fallback: current directory
    return SDL_strdup("./");
}

static char *
join_path(const char *dir, const char *name) {
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    bool need_sep = (dlen > 0) && (dir[dlen - 1] != '/' && dir[dlen - 1] != '\\');
    char *out = malloc(dlen + nlen + (need_sep ? 2 : 1));
    if (!out) return NULL;
    memcpy(out, dir, dlen);
    if (need_sep) out[dlen++] = '/';
    memcpy(out + dlen, name, nlen + 1);
    return out;
}

static time_t
file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

static char *
read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

// ============================================================
// Key name <-> keycode
// ============================================================

SDL_Keycode
sc_mapping_parse_key_name(const char *name) {
    char buf[64];
    strncpy(buf, name, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *start = buf;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    if (*start == '\0') return SDLK_UNKNOWN;

    if (SDL_strcasecmp(start, "space") == 0) return SDLK_SPACE;
    if (SDL_strcasecmp(start, "lshift") == 0 ||
        SDL_strcasecmp(start, "left shift") == 0 ||
        SDL_strcasecmp(start, "shift") == 0) return SDLK_LSHIFT;
    if (SDL_strcasecmp(start, "rshift") == 0 ||
        SDL_strcasecmp(start, "right shift") == 0) return SDLK_RSHIFT;
    if (SDL_strcasecmp(start, "lctrl") == 0 ||
        SDL_strcasecmp(start, "left ctrl") == 0 ||
        SDL_strcasecmp(start, "ctrl") == 0) return SDLK_LCTRL;
    if (SDL_strcasecmp(start, "rctrl") == 0 ||
        SDL_strcasecmp(start, "right ctrl") == 0) return SDLK_RCTRL;
    if (SDL_strcasecmp(start, "lalt") == 0 ||
        SDL_strcasecmp(start, "left alt") == 0 ||
        SDL_strcasecmp(start, "alt") == 0) return SDLK_LALT;
    if (SDL_strcasecmp(start, "ralt") == 0 ||
        SDL_strcasecmp(start, "right alt") == 0) return SDLK_RALT;
    if (SDL_strcasecmp(start, "esc") == 0 ||
        SDL_strcasecmp(start, "escape") == 0) return SDLK_ESCAPE;
    if (SDL_strcasecmp(start, "enter") == 0 ||
        SDL_strcasecmp(start, "return") == 0) return SDLK_RETURN;
    if (SDL_strcasecmp(start, "tab") == 0) return SDLK_TAB;
    if (SDL_strcasecmp(start, "backspace") == 0) return SDLK_BACKSPACE;

    if (strlen(start) == 1) {
        char c = toupper((unsigned char)start[0]);
        if (c >= 'A' && c <= 'Z') return SDLK_A + (c - 'A');
        if (c >= '0' && c <= '9') return SDLK_0 + (c - '0');
    }

    SDL_Keycode kc = SDL_GetKeyFromName(start);
    if (kc != SDLK_UNKNOWN) return kc;

    char upper[64];
    for (int i = 0; start[i]; i++) upper[i] = toupper((unsigned char)start[i]);
    upper[strlen(start)] = '\0';
    kc = SDL_GetKeyFromName(upper);
    if (kc != SDLK_UNKNOWN) return kc;

    return SDLK_UNKNOWN;
}

const char *
sc_mapping_key_name(SDL_Keycode kc) {
    const char *n = SDL_GetKeyName(kc);
    if (n && *n) return n;
    return "Unknown";
}

// ============================================================
// Entry management
// ============================================================

static void
entry_free(struct sc_mapping_entry *e) {
    if (!e) return;
    free(e->key_name);
    e->key_name = NULL;
}

static bool
entry_set(struct sc_mapping_entry *e, const char *key_name,
          SDL_Keycode *keys, int nkeys, double nx, double ny,
          uint64_t pointer_id) {
    e->key_name = strdup(key_name);
    if (!e->key_name) return false;
    if (nkeys > SC_MAPPING_MAX_KEYS) nkeys = SC_MAPPING_MAX_KEYS;
    memcpy(e->keys, keys, nkeys * sizeof(SDL_Keycode));
    e->nkeys = nkeys;
    e->nx = nx;
    e->ny = ny;
    e->pointer_id = pointer_id;
    e->down = false;
    e->active = false;
    return true;
}

static void
clear_entries(struct sc_mapping_manager *mm) {
    for (int i = 0; i < mm->count; i++) {
        entry_free(&mm->entries[i]);
    }
    mm->count = 0;
    mm->mouse_right = NULL;
    mm->mouse_left_locked = NULL;
}

static struct sc_mapping_entry *
alloc_entry(struct sc_mapping_manager *mm) {
    if (mm->count >= mm->capacity) {
        int new_cap = mm->capacity ? mm->capacity * 2 : 16;
        struct sc_mapping_entry *na = realloc(mm->entries,
                                              new_cap * sizeof(*na));
        if (!na) return NULL;
        mm->entries = na;
        mm->capacity = new_cap;
    }
    struct sc_mapping_entry *e = &mm->entries[mm->count];
    memset(e, 0, sizeof(*e));
    mm->count++;
    return e;
}

// ============================================================
// Special key classification
// ============================================================

static bool
is_mouse_right(const char *name) {
    return SDL_strcasecmp(name, "MOUSE_RIGHT") == 0
        || SDL_strcasecmp(name, "RIGHT_CLICK") == 0;
}

static bool
is_mouse_left_locked(const char *name) {
    return SDL_strcasecmp(name, "MOUSE_LEFT_LOCKED") == 0
        || SDL_strcasecmp(name, "LEFT_CLICK_LOCKED") == 0;
}

// ============================================================
// JSON template parsing
// ============================================================

static bool
parse_combo_keys(const char *key_name, SDL_Keycode *out, int *nkeys_out) {
    if (!strchr(key_name, '+')) {
        // single key
        SDL_Keycode kc = sc_mapping_parse_key_name(key_name);
        if (kc == SDLK_UNKNOWN) return false;
        out[0] = kc;
        *nkeys_out = 1;
        return true;
    }
    // combo
    char tmp[128];
    strncpy(tmp, key_name, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    int nkeys = 0;
    char *save = NULL;
    char *tok = strtok_r(tmp, "+", &save);
    while (tok && nkeys < SC_MAPPING_MAX_KEYS) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *e = tok + strlen(tok);
        while (e > tok && isspace((unsigned char)*(e - 1))) e--;
        *e = '\0';
        SDL_Keycode kc = sc_mapping_parse_key_name(tok);
        if (kc == SDLK_UNKNOWN) {
            LOGW("Unknown combo key: %s", tok);
            return false;
        }
        out[nkeys++] = kc;
        tok = strtok_r(NULL, "+", &save);
    }
    if (nkeys < 2) return false;
    *nkeys_out = nkeys;
    return true;
}

static bool
load_template_json(struct sc_mapping_manager *mm, const char *path) {
    char *text = read_file(path);
    if (!text) {
        LOGW("Could not open template: %s", path);
        return false;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        LOGW("Failed to parse template JSON: %s", cJSON_GetErrorPtr());
        return false;
    }

    clear_entries(mm);

    cJSON *mappings = cJSON_GetObjectItemCaseSensitive(root, "mappings");
    if (cJSON_IsArray(mappings)) {
        int n = cJSON_GetArraySize(mappings);
        for (int i = 0; i < n; i++) {
            cJSON *item = cJSON_GetArrayItem(mappings, i);
            cJSON *jk = cJSON_GetObjectItemCaseSensitive(item, "key");
            cJSON *jx = cJSON_GetObjectItemCaseSensitive(item, "x");
            cJSON *jy = cJSON_GetObjectItemCaseSensitive(item, "y");
            if (!cJSON_IsString(jk) || !cJSON_IsNumber(jx) || !cJSON_IsNumber(jy)) {
                continue;
            }
            const char *key_name = jk->valuestring;
            double nx = jx->valuedouble;
            double ny = jy->valuedouble;

            // Clamp to [0,1]
            if (nx < 0.0) nx = 0.0;
            if (nx > 1.0) nx = 1.0;
            if (ny < 0.0) ny = 0.0;
            if (ny > 1.0) ny = 1.0;

            // Special mouse entries
            if (is_mouse_right(key_name) || is_mouse_left_locked(key_name)) {
                struct sc_mapping_entry *e = alloc_entry(mm);
                if (!e) continue;
                SDL_Keycode dummy = SDLK_UNKNOWN;
                if (!entry_set(e, key_name, &dummy, 0, nx, ny,
                               (uint64_t)-(20 + mm->count))) {
                    mm->count--;
                    continue;
                }
                if (is_mouse_right(key_name)) {
                    mm->mouse_right = e;
                } else {
                    mm->mouse_left_locked = e;
                }
                continue;
            }

            SDL_Keycode keys[SC_MAPPING_MAX_KEYS];
            int nkeys = 0;
            if (!parse_combo_keys(key_name, keys, &nkeys)) {
                LOGW("Invalid key in template: %s", key_name);
                continue;
            }
            struct sc_mapping_entry *e = alloc_entry(mm);
            if (!e) continue;
            uint64_t pid = (nkeys >= 2)
                         ? (uint64_t)-(100 + mm->count)
                         : (uint64_t)-(10 + mm->count);
            if (!entry_set(e, key_name, keys, nkeys, nx, ny, pid)) {
                mm->count--;
                continue;
            }
        }
    }

    cJSON_Delete(root);
    LOGI("Loaded template '%s': %d entries", mm->active_name ? mm->active_name : "?", mm->count);
    return true;
}

// ============================================================
// JSON template saving
// ============================================================

bool
sc_mapping_manager_save_active(struct sc_mapping_manager *mm) {
    if (!mm->active_name) return false;
    char *path = join_path(mm->templates_dir, mm->active_name);
    if (!path) return false;

    char *full = malloc(strlen(path) + 6); // ".json" + \0
    if (!full) { free(path); return false; }
    snprintf(full, strlen(path) + 6, "%s.json", path);
    free(path);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", mm->active_name);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < mm->count; i++) {
        struct sc_mapping_entry *e = &mm->entries[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "key", e->key_name);
        cJSON_AddNumberToObject(item, "x", e->nx);
        cJSON_AddNumberToObject(item, "y", e->ny);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(root, "mappings", arr);

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) { free(full); return false; }

    FILE *f = fopen(full, "wb");
    bool ok = false;
    if (f) {
        fputs(text, f);
        fclose(f);
        ok = true;
        mm->active_mtime = file_mtime(full);
        LOGI("Saved template '%s'", mm->active_name);
    } else {
        LOGW("Could not write template: %s", full);
    }
    cJSON_free(text);
    free(full);
    return ok;
}

// ============================================================
// Config (shortcuts + active template)
// ============================================================

static void
load_config(struct sc_mapping_manager *mm) {
    char *text = read_file(mm->config_path);
    if (!text) {
        LOGI("No config file found, using defaults");
        return;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        LOGW("Failed to parse config JSON");
        return;
    }

    cJSON *shortcuts = cJSON_GetObjectItemCaseSensitive(root, "shortcuts");
    if (cJSON_IsObject(shortcuts)) {
        cJSON *sw = cJSON_GetObjectItemCaseSensitive(shortcuts, "switch_template");
        cJSON *ds = cJSON_GetObjectItemCaseSensitive(shortcuts, "design_mode");
        cJSON *tg = cJSON_GetObjectItemCaseSensitive(shortcuts, "toggle_mapping");
        if (cJSON_IsString(sw)) {
            SDL_Keycode kc = sc_mapping_parse_key_name(sw->valuestring);
            if (kc != SDLK_UNKNOWN) mm->sc_switch = kc;
        }
        if (cJSON_IsString(ds)) {
            SDL_Keycode kc = sc_mapping_parse_key_name(ds->valuestring);
            if (kc != SDLK_UNKNOWN) mm->sc_design = kc;
        }
        if (cJSON_IsString(tg)) {
            SDL_Keycode kc = sc_mapping_parse_key_name(tg->valuestring);
            if (kc != SDLK_UNKNOWN) mm->sc_toggle = kc;
        }
    }

    cJSON *active = cJSON_GetObjectItemCaseSensitive(root, "active_template");
    if (cJSON_IsString(active)) {
        free(mm->active_name);
        mm->active_name = strdup(active->valuestring);
    }

    cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(en)) {
        mm->enabled = cJSON_IsTrue(en);
    }

    cJSON_Delete(root);
    mm->config_mtime = file_mtime(mm->config_path);
}

static bool
save_config(struct sc_mapping_manager *mm) {
    cJSON *root = cJSON_CreateObject();
    cJSON *shortcuts = cJSON_CreateObject();
    cJSON_AddStringToObject(shortcuts, "switch_template",
                            sc_mapping_key_name(mm->sc_switch));
    cJSON_AddStringToObject(shortcuts, "design_mode",
                            sc_mapping_key_name(mm->sc_design));
    cJSON_AddStringToObject(shortcuts, "toggle_mapping",
                            sc_mapping_key_name(mm->sc_toggle));
    cJSON_AddItemToObject(root, "shortcuts", shortcuts);
    if (mm->active_name) {
        cJSON_AddStringToObject(root, "active_template", mm->active_name);
    }
    cJSON_AddBoolToObject(root, "enabled", mm->enabled);

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return false;

    FILE *f = fopen(mm->config_path, "wb");
    bool ok = false;
    if (f) {
        fputs(text, f);
        fclose(f);
        ok = true;
        mm->config_mtime = file_mtime(mm->config_path);
    }
    cJSON_free(text);
    return ok;
}

// ============================================================
// Template listing
// ============================================================

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

int
sc_mapping_manager_list(struct sc_mapping_manager *mm, char **names, int max) {
    int total = 0;

#ifdef _WIN32
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s*.json", mm->templates_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char *fn = fd.cFileName;
        size_t len = strlen(fn);
        if (len < 5 || SDL_strcasecmp(fn + len - 5, ".json") != 0) continue;
        if (total < max && names) {
            names[total] = malloc(len - 4);
            if (names[total]) {
                memcpy(names[total], fn, len - 5);
                names[total][len - 5] = '\0';
            }
        }
        total++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(mm->templates_dir);
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *fn = ent->d_name;
        size_t len = strlen(fn);
        if (len < 5 || SDL_strcasecmp(fn + len - 5, ".json") != 0) continue;
        if (total < max && names) {
            names[total] = malloc(len - 4);
            if (names[total]) {
                memcpy(names[total], fn, len - 5);
                names[total][len - 5] = '\0';
            }
        }
        total++;
    }
    closedir(d);
#endif
    return total;
}

// ============================================================
// Core operations
// ============================================================

static bool
load_active(struct sc_mapping_manager *mm) {
    if (!mm->active_name) return false;
    char *path = join_path(mm->templates_dir, mm->active_name);
    if (!path) return false;
    char *full = malloc(strlen(path) + 6);
    if (!full) { free(path); return false; }
    snprintf(full, strlen(path) + 6, "%s.json", path);
    free(path);

    bool ok = load_template_json(mm, full);
    mm->active_mtime = file_mtime(full);
    free(full);
    return ok;
}

bool
sc_mapping_manager_init(struct sc_mapping_manager *mm) {
    memset(mm, 0, sizeof(*mm));

    char *base = sc_mapping_get_base_dir();
    mm->templates_dir = join_path(base, "templates/");
    mm->config_path = join_path(base, "scrcpy_mapping_config.json");
    SDL_free(base);

    if (!mm->templates_dir || !mm->config_path) {
        LOGE("Could not resolve mapping paths");
        return false;
    }

    // Default shortcuts
    mm->sc_switch = SDLK_F2;
    mm->sc_design = SDLK_F3;
    mm->sc_toggle = SDLK_F4;
    mm->enabled = true;

    load_config(mm);

    if (!mm->active_name) {
        mm->active_name = strdup("default");
    }

    if (!load_active(mm)) {
        LOGW("Active template not found, mapping disabled");
        mm->enabled = false;
    }

    return true;
}

void
sc_mapping_manager_reload_if_changed(struct sc_mapping_manager *mm) {
    // Check config for shortcut/active-template changes
    time_t cm = file_mtime(mm->config_path);
    if (cm != 0 && cm != mm->config_mtime) {
        LOGI("Config changed, reloading");
        char *old_active = mm->active_name ? strdup(mm->active_name) : NULL;
        load_config(mm);
        // If active template changed, reload it
        if (!old_active || !mm->active_name ||
            strcmp(old_active, mm->active_name) != 0) {
            load_active(mm);
        }
        free(old_active);
    }

    // Check active template file for content changes (hot-reload)
    if (mm->active_name) {
        char *path = join_path(mm->templates_dir, mm->active_name);
        if (path) {
            char *full = malloc(strlen(path) + 6);
            if (full) {
                snprintf(full, strlen(path) + 6, "%s.json", path);
                time_t tm = file_mtime(full);
                if (tm != 0 && tm != mm->active_mtime) {
                    LOGI("Template '%s' changed, hot-reloading", mm->active_name);
                    load_active(mm);
                }
                free(full);
            }
            free(path);
        }
    }
}

bool
sc_mapping_manager_switch(struct sc_mapping_manager *mm, const char *name) {
    if (!name) return false;
    free(mm->active_name);
    mm->active_name = strdup(name);
    bool ok = load_active(mm);
    if (ok) {
        save_config(mm);
    }
    return ok;
}

void
sc_mapping_manager_set_enabled(struct sc_mapping_manager *mm, bool enabled) {
    mm->enabled = enabled;
    save_config(mm);
    LOGI("Key mapping %s", enabled ? "ENABLED" : "DISABLED");
}

// ============================================================
// Lookup helpers
// ============================================================

struct sc_mapping_entry *
sc_mapping_manager_find_single(struct sc_mapping_manager *mm, SDL_Keycode kc) {
    for (int i = 0; i < mm->count; i++) {
        struct sc_mapping_entry *e = &mm->entries[i];
        if (e->nkeys == 1 && e->keys[0] == kc) return e;
    }
    return NULL;
}

struct sc_mapping_entry *
sc_mapping_manager_find_by_name(struct sc_mapping_manager *mm,
                                const char *key_name) {
    for (int i = 0; i < mm->count; i++) {
        if (mm->entries[i].key_name &&
            SDL_strcasecmp(mm->entries[i].key_name, key_name) == 0) {
            return &mm->entries[i];
        }
    }
    return NULL;
}

bool
sc_mapping_manager_key_in_combo(struct sc_mapping_manager *mm, SDL_Keycode kc) {
    for (int i = 0; i < mm->count; i++) {
        struct sc_mapping_entry *e = &mm->entries[i];
        if (e->nkeys >= 2) {
            for (int j = 0; j < e->nkeys; j++) {
                if (e->keys[j] == kc) return true;
            }
        }
    }
    return false;
}

// ============================================================
// Design mode
// ============================================================

void
sc_mapping_manager_design_set_pending(struct sc_mapping_manager *mm,
                                      double nx, double ny) {
    mm->design_has_pending = true;
    mm->design_nx = nx;
    mm->design_ny = ny;
}

bool
sc_mapping_manager_design_bind(struct sc_mapping_manager *mm,
                               const char *key_name) {
    if (!mm->design_has_pending) return false;
    if (!key_name || !*key_name) return false;

    // If entry exists, update its position
    struct sc_mapping_entry *existing = sc_mapping_manager_find_by_name(mm, key_name);
    if (existing) {
        existing->nx = mm->design_nx;
        existing->ny = mm->design_ny;
        mm->design_has_pending = false;
        return true;
    }

    // Special mouse entries
    if (is_mouse_right(key_name) || is_mouse_left_locked(key_name)) {
        struct sc_mapping_entry *e = alloc_entry(mm);
        if (!e) return false;
        SDL_Keycode dummy = SDLK_UNKNOWN;
        if (!entry_set(e, key_name, &dummy, 0, mm->design_nx, mm->design_ny,
                       (uint64_t)-(20 + mm->count))) {
            mm->count--;
            return false;
        }
        if (is_mouse_right(key_name)) mm->mouse_right = e;
        else mm->mouse_left_locked = e;
        mm->design_has_pending = false;
        return true;
    }

    SDL_Keycode keys[SC_MAPPING_MAX_KEYS];
    int nkeys = 0;
    if (!parse_combo_keys(key_name, keys, &nkeys)) return false;

    struct sc_mapping_entry *e = alloc_entry(mm);
    if (!e) return false;
    uint64_t pid = (nkeys >= 2) ? (uint64_t)-(100 + mm->count)
                                : (uint64_t)-(10 + mm->count);
    if (!entry_set(e, key_name, keys, nkeys, mm->design_nx, mm->design_ny, pid)) {
        mm->count--;
        return false;
    }
    mm->design_has_pending = false;
    return true;
}

bool
sc_mapping_manager_remove_entry(struct sc_mapping_manager *mm,
                                const char *key_name) {
    for (int i = 0; i < mm->count; i++) {
        if (mm->entries[i].key_name &&
            SDL_strcasecmp(mm->entries[i].key_name, key_name) == 0) {
            entry_free(&mm->entries[i]);
            // shift remaining
            memmove(&mm->entries[i], &mm->entries[i + 1],
                    (mm->count - i - 1) * sizeof(struct sc_mapping_entry));
            mm->count--;
            // Re-resolve special mouse pointers
            mm->mouse_right = NULL;
            mm->mouse_left_locked = NULL;
            for (int j = 0; j < mm->count; j++) {
                if (is_mouse_right(mm->entries[j].key_name))
                    mm->mouse_right = &mm->entries[j];
                else if (is_mouse_left_locked(mm->entries[j].key_name))
                    mm->mouse_left_locked = &mm->entries[j];
            }
            return true;
        }
    }
    return false;
}

// ============================================================
// Destroy
// ============================================================

void
sc_mapping_manager_destroy(struct sc_mapping_manager *mm) {
    clear_entries(mm);
    free(mm->entries);
    mm->entries = NULL;
    mm->capacity = 0;
    free(mm->active_name);
    mm->active_name = NULL;
    free(mm->templates_dir);
    mm->templates_dir = NULL;
    free(mm->config_path);
    mm->config_path = NULL;
}
