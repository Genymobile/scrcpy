#include "intmap.h"

const struct sc_intmap_entry *
sc_intmap_find_entry(const struct sc_intmap_entry entries[], size_t len,
                     int32_t key) {
    for (size_t i = 0; i < len; ++i) {
        const struct sc_intmap_entry *entry = &entries[i];
        if (entry->key == key) {
            return entry;
        }
    }
    return NULL;
}
