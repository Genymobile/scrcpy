#include "ime_position.h"

#import <AppKit/AppKit.h>

#include <SDL3/SDL_properties.h>

bool
sc_macos_ime_invalidate_character_coordinates(SDL_Window *window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    NSWindow *nswindow = (NSWindow *) SDL_GetPointerProperty(
            props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    NSResponder *responder = nswindow.firstResponder;
    if ([responder respondsToSelector:@selector(inputContext)]) {
        NSTextInputContext *context = [(NSView *) responder inputContext];
        if (context) {
            [context invalidateCharacterCoordinates];
            return true;
        }
    }
    return false;
}
