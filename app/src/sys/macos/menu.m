#include "menu.h"

#import <AppKit/AppKit.h>

#include "events.h"
#include "input_manager.h"
#include "screen.h"

@interface SCPowerMenuTarget : NSObject <NSMenuItemValidation>
@property(nonatomic, assign) struct sc_input_manager *inputManager;
@end

@implementation SCPowerMenuTarget

- (void)pressPower:(id)sender {
    (void) sender;
    // Route the action through the SDL event loop, like keyboard input.
    sc_push_event(SC_EVENT_POWER_BUTTON);
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    (void) item;
    return self.inputManager
        && sc_input_manager_can_press_power(self.inputManager);
}

@end

static NSMenuItem *device_menu_item;
static SCPowerMenuTarget *target;

bool
sc_macos_menu_init(struct sc_screen *screen) {
    NSMenu *main_menu = NSApp.mainMenu;
    if (!main_menu) {
        return false;
    }

    target = [[SCPowerMenuTarget alloc] init];
    target.inputManager = &screen->im;

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Device"];
    NSMenuItem *power = [[NSMenuItem alloc] initWithTitle:@"Power Button"
                                                action:@selector(pressPower:)
                                         keyEquivalent:@""];
    power.target = target;
    [menu addItem:power];

    device_menu_item = [[NSMenuItem alloc] initWithTitle:@"Device"
                                               action:nil
                                        keyEquivalent:@""];
    device_menu_item.submenu = menu;
    [main_menu addItem:device_menu_item];
    return true;
}

void
sc_macos_menu_destroy(void) {
    [device_menu_item.menu removeItem:device_menu_item];
    target.inputManager = NULL;
    device_menu_item = nil;
    target = nil;
}
