#include <assert.h>
#include <stdbool.h>

#include "keyboard_shortcuts.h"

static void
test_page_up_down_scroll_shortcuts(void) {
    float hscroll;
    float vscroll;

    bool ok = sc_keyboard_shortcut_to_scroll(SDLK_PAGEUP, false,
                                             &hscroll, &vscroll);
    assert(ok);
    assert(hscroll == 0.0f);
    assert(vscroll == 1.0f);

    ok = sc_keyboard_shortcut_to_scroll(SDLK_PAGEDOWN, false,
                                        &hscroll, &vscroll);
    assert(ok);
    assert(hscroll == 0.0f);
    assert(vscroll == -1.0f);
}

static void
test_scroll_shortcuts_ignore_shift(void) {
    float hscroll;
    float vscroll;

    bool ok = sc_keyboard_shortcut_to_scroll(SDLK_PAGEUP, true,
                                             &hscroll, &vscroll);
    assert(!ok);

    ok = sc_keyboard_shortcut_to_scroll(SDLK_PAGEDOWN, true,
                                        &hscroll, &vscroll);
    assert(!ok);
}

int
main(void) {
    test_page_up_down_scroll_shortcuts();
    test_scroll_shortcuts_ignore_shift();
    return 0;
}
