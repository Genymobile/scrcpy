#ifndef SC_MAPPING_MANAGER_H
#define SC_MAPPING_MANAGER_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <SDL3/SDL_keycode.h>

#define SC_MAPPING_MAX_KEYS 4

// A single mapping entry: one key or a key-combo maps to a normalized
// screen position (nx, ny in [0.0, 1.0]).
struct sc_mapping_entry {
    char *key_name;                              // e.g. "A", "W+D", "MOUSE_RIGHT"
    SDL_Keycode keys[SC_MAPPING_MAX_KEYS];       // parsed keycodes
    int nkeys;
    double nx;
    double ny;
    uint64_t pointer_id;
    bool down;     // current pressed state (single key)
    bool active;   // current active state (combo)
};

// Special mouse mapping kinds
enum sc_mapping_mouse_kind {
    SC_MAPPING_MOUSE_NONE = 0,
    SC_MAPPING_MOUSE_RIGHT,
    SC_MAPPING_MOUSE_LEFT_LOCKED,
};

struct sc_mapping_manager {
    // Active template entries (parsed, ready for input handling)
    struct sc_mapping_entry *entries;
    int count;
    int capacity;

    // Pre-resolved special mouse mappings (pointer into entries)
    struct sc_mapping_entry *mouse_right;
    struct sc_mapping_entry *mouse_left_locked;

    // Global state
    bool enabled;
    char *active_name;   // current active template name

    // Shortcuts (SDL keycodes)
    SDL_Keycode sc_switch;    // quick template switch
    SDL_Keycode sc_design;    // toggle design mode
    SDL_Keycode sc_toggle;    // toggle mapping on/off

    // File paths
    char *templates_dir;   // .../templates/
    char *config_path;     // .../scrcpy_mapping_config.json

    // Hot-reload tracking
    time_t active_mtime;
    time_t config_mtime;

    // Design-mode scratch: the position last clicked (normalized)
    bool design_has_pending;
    double design_nx;
    double design_ny;
};

// Initialize the manager: resolves paths, loads config, loads active template.
// Returns false on fatal failure (manager is still usable but disabled).
bool
sc_mapping_manager_init(struct sc_mapping_manager *mm);

// Check for external file changes and reload if necessary.
// Call periodically from the event loop.
void
sc_mapping_manager_reload_if_changed(struct sc_mapping_manager *mm);

// Switch the active template by name. Writes the choice to the config file.
bool
sc_mapping_manager_switch(struct sc_mapping_manager *mm, const char *name);

// Enable/disable all custom key mapping.
void
sc_mapping_manager_set_enabled(struct sc_mapping_manager *mm, bool enabled);

// List available template names. Writes up to max pointers into names.
// Caller must free each string. Returns the total number of templates found.
int
sc_mapping_manager_list(struct sc_mapping_manager *mm, char **names, int max);

// Find a single-key entry by keycode (returns NULL if not found).
struct sc_mapping_entry *
sc_mapping_manager_find_single(struct sc_mapping_manager *mm, SDL_Keycode kc);

// Find an entry by its key name string.
struct sc_mapping_entry *
sc_mapping_manager_find_by_name(struct sc_mapping_manager *mm,
                                const char *key_name);

// Check whether a keycode participates in any combo.
bool
sc_mapping_manager_key_in_combo(struct sc_mapping_manager *mm, SDL_Keycode kc);

// ---- Design mode helpers ----

// Record a pending normalized position from a screen click.
void
sc_mapping_manager_design_set_pending(struct sc_mapping_manager *mm,
                                      double nx, double ny);

// Bind the pending position to the given key name. Adds/updates an entry.
bool
sc_mapping_manager_design_bind(struct sc_mapping_manager *mm,
                               const char *key_name);

// Remove the entry for the given key name.
bool
sc_mapping_manager_remove_entry(struct sc_mapping_manager *mm,
                                const char *key_name);

// Save the current active template to its JSON file.
bool
sc_mapping_manager_save_active(struct sc_mapping_manager *mm);

// Destroy and free all resources.
void
sc_mapping_manager_destroy(struct sc_mapping_manager *mm);

// ---- Key name <-> keycode helpers (shared with overlay/design) ----

// Parse a key name (e.g. "A", "Space", "F2", "Left Shift") into an SDL keycode.
SDL_Keycode
sc_mapping_parse_key_name(const char *name);

// Get a human-readable name for an SDL keycode.
const char *
sc_mapping_key_name(SDL_Keycode kc);

// Resolve the base directory for config/templates (next to the executable).
// Caller must free the result.
char *
sc_mapping_get_base_dir(void);

#endif
