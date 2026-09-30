#import <AppKit/AppKit.h>

#include "clipboard_writer.h"

#include <SDL3/SDL.h>

bool
sc_clipboard_write_png(void *data, size_t size) {
    @autoreleasepool {
        // SDL's Cocoa backend uses a lazy pasteboard provider. Replacing the
        // clipboard may release its userdata before AppKit requests the old
        // data, so publish an eager NSData representation on macOS instead.
        NSData *png = [NSData dataWithBytes:data length:size];
        SDL_free(data);
        if (!png) {
            return SDL_SetError("Could not create PNG clipboard data");
        }

        NSPasteboardItem *item = [[NSPasteboardItem alloc] init];
        if (![item setData:png forType:NSPasteboardTypePNG]) {
            return SDL_SetError("Could not create PNG clipboard item");
        }

        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        if (![pasteboard writeObjects:@[item]]) {
            return SDL_SetError("Could not write PNG clipboard data");
        }
        return true;
    }
}
