#include "env.h"

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
# include "util/str.h"
#endif

char *
sc_get_env(const char *varname) {
#ifdef _WIN32
    wchar_t *w_varname = sc_str_to_wchars(varname);
    if (!w_varname) {
         return NULL;
    }
    const wchar_t *value = _wgetenv(w_varname);
    free(w_varname);
    if (!value) {
        return NULL;
    }

    return sc_str_from_wchars(value);
#else
    const char *value = getenv(varname);
    if (!value) {
        return NULL;
    }

    return strdup(value);
#endif
}
