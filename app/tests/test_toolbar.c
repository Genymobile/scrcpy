#include "common.h"

#include <assert.h>
#include "icon.h"
#include "toolbar.h"

SDL_Surface *
sc_icon_load(const char *filename) {
    (void) filename;
    return NULL;
}

void
sc_icon_destroy(SDL_Surface *icon) {
    (void) icon;
}

static void
test_toolbar_layout_and_hit_test(void) {
    struct sc_toolbar toolbar;
    sc_toolbar_init(&toolbar, true, false);
    sc_toolbar_layout(&toolbar);

    assert(!toolbar.user_visible);
    assert(toolbar.panel.w == SC_TOOLBAR_PANEL_WIDTH);
    assert(toolbar.panel.h == SC_TOOLBAR_PANEL_HEIGHT);
    assert(SC_TOOLBAR_PANEL_WIDTH == 48);
    assert(SC_TOOLBAR_BUTTON_HEIGHT == 48);
    assert((SC_TOOLBAR_PANEL_WIDTH - SC_TOOLBAR_ICON_SIZE) / 2 == 16);
    assert((SC_TOOLBAR_BUTTON_HEIGHT - SC_TOOLBAR_ICON_SIZE) / 2 == 16);
    assert(toolbar.buttons[SC_TOOLBAR_ACTION_HOME].y
           < toolbar.buttons[SC_TOOLBAR_ACTION_BACK].y);
    assert(toolbar.buttons[SC_TOOLBAR_ACTION_BACK].y
           < toolbar.buttons[SC_TOOLBAR_ACTION_RECENTS].y);
    assert(toolbar.buttons[SC_TOOLBAR_ACTION_RECENTS].y
           < toolbar.buttons[SC_TOOLBAR_ACTION_SCREENSHOT].y);
    assert(toolbar.buttons[SC_TOOLBAR_ACTION_SCREENSHOT].y
           < toolbar.buttons[SC_TOOLBAR_ACTION_RECORD].y);
    assert(toolbar.buttons[SC_TOOLBAR_ACTION_SCREENSHOT].y
           - (toolbar.buttons[SC_TOOLBAR_ACTION_RECENTS].y
              + SC_TOOLBAR_BUTTON_HEIGHT) == SC_TOOLBAR_SEPARATOR_HEIGHT);

    for (enum sc_toolbar_action action = SC_TOOLBAR_ACTION_HOME;
            action < SC_TOOLBAR_ACTION_COUNT; ++action) {
        SDL_FRect rect = toolbar.buttons[action];
        enum sc_toolbar_action hit =
            sc_toolbar_hit_test(&toolbar, rect.x + rect.w / 2.f,
                                rect.y + rect.h / 2.f);
        assert(hit == action);
    }

    assert(sc_toolbar_hit_test(&toolbar, 0.f, 0.f)
           == SC_TOOLBAR_ACTION_NONE);
    assert(!sc_toolbar_contains(&toolbar, toolbar.panel.x,
                                toolbar.panel.y));
    assert(!sc_toolbar_contains(&toolbar, toolbar.panel.x + 1.f,
                                toolbar.panel.y + toolbar.panel.h - 1.f));
}

static void
test_toolbar_initial_visibility(void) {
    struct sc_toolbar toolbar;

    sc_toolbar_init(&toolbar, true, false);
    assert(!toolbar.user_visible);

    sc_toolbar_init(&toolbar, true, true);
    assert(toolbar.user_visible);

    sc_toolbar_init(&toolbar, false, true);
    assert(!toolbar.user_visible);
}

static void
test_disabled_toolbar(void) {
    struct sc_toolbar toolbar;
    sc_toolbar_init(&toolbar, false, false);
    sc_toolbar_layout(&toolbar);

    assert(!sc_toolbar_contains(&toolbar, 10.f, 10.f));
    assert(sc_toolbar_hit_test(&toolbar, toolbar.panel.x + 30.f,
                               toolbar.panel.y + 36.f)
           == SC_TOOLBAR_ACTION_NONE);
}

int
main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_toolbar_layout_and_hit_test();
    test_toolbar_initial_visibility();
    test_disabled_toolbar();
    return 0;
}
