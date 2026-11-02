#include "input_source.h"

#ifdef __APPLE__
# include <Carbon/Carbon.h>
#endif

bool
sc_input_source_is_input_method(bool *input_method) {
#ifdef __APPLE__
    // Must be called from the main thread
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    if (!source) {
        return false;
    }

    CFStringRef type =
        TISGetInputSourceProperty(source, kTISPropertyInputSourceType);
    bool ok = type != NULL;
    if (ok) {
        *input_method = !CFEqual(type, kTISTypeKeyboardLayout);
    }

    CFRelease(source);
    return ok;
#else
    (void) input_method;
    return false;
#endif
}
