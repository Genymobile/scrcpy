#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include "window_ui.h"

@interface SCScrcpyWindowUI : NSObject
@property(nonatomic, strong) NSStackView *titleStack;
@property(nonatomic, strong) NSTextField *primaryLabel;
@property(nonatomic, strong) NSTextField *secondaryLabel;
@property(nonatomic, strong) NSPanel *toastPanel;
@property(nonatomic) NSUInteger toastGeneration;
@property(nonatomic) sc_window_ui_toolbar_toggle_cb toolbarToggleCallback;
@property(nonatomic) void *toolbarToggleUserdata;
@property(nonatomic, strong) NSMenuItem *toolbarMenuItem;
@property(nonatomic, strong) NSMenuItem *toolbarMenuContainer;
@property(nonatomic) BOOL ownsToolbarMenuContainer;
@end

@implementation SCScrcpyWindowUI
- (void)toggleToolbar:(id)sender {
    (void) sender;
    if (self.toolbarToggleCallback) {
        self.toolbarToggleCallback(self.toolbarToggleUserdata);
    }
}
@end

static const void *SC_WINDOW_UI_KEY = &SC_WINDOW_UI_KEY;

static NSWindow *
sc_window_ui_get_native_window(struct sc_window_ui *ui) {
    SDL_PropertiesID props = SDL_GetWindowProperties(ui->window);
    void *pointer = SDL_GetPointerProperty(
        props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    return (__bridge NSWindow *) pointer;
}

static NSTextField *
sc_window_ui_create_label(void) {
    NSTextField *label = [NSTextField labelWithString:@""];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.maximumNumberOfLines = 1;
    return label;
}

static SCScrcpyWindowUI *
sc_window_ui_get_controller(NSWindow *window) {
    SCScrcpyWindowUI *ui = objc_getAssociatedObject(window, SC_WINDOW_UI_KEY);
    if (ui) {
        return ui;
    }

    ui = [[SCScrcpyWindowUI alloc] init];
    objc_setAssociatedObject(window, SC_WINDOW_UI_KEY, ui,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return ui;
}

static SCScrcpyWindowUI *
sc_window_ui_get_or_create_title(NSWindow *window) {
    SCScrcpyWindowUI *ui = sc_window_ui_get_controller(window);
    if (ui.titleStack) {
        return ui;
    }

    NSButton *closeButton = [window standardWindowButton:NSWindowCloseButton];
    NSView *titlebar = closeButton.superview;
    if (!titlebar) {
        return ui;
    }

    ui.primaryLabel = sc_window_ui_create_label();
    ui.primaryLabel.font = [NSFont systemFontOfSize:14
                                            weight:NSFontWeightSemibold];
    ui.primaryLabel.textColor = NSColor.labelColor;
    [ui.primaryLabel
        setContentCompressionResistancePriority:NSLayoutPriorityDefaultHigh
        forOrientation:NSLayoutConstraintOrientationHorizontal];

    ui.secondaryLabel = sc_window_ui_create_label();
    ui.secondaryLabel.font = [NSFont systemFontOfSize:10
                                              weight:NSFontWeightRegular];
    ui.secondaryLabel.textColor = NSColor.secondaryLabelColor;
    ui.secondaryLabel.alphaValue = .68;
    [ui.secondaryLabel
        setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
        forOrientation:NSLayoutConstraintOrientationHorizontal];

    ui.titleStack = [NSStackView stackViewWithViews:@[
        ui.primaryLabel, ui.secondaryLabel
    ]];
    ui.titleStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    ui.titleStack.alignment = NSLayoutAttributeCenterY;
    ui.titleStack.spacing = 6;
    ui.titleStack.translatesAutoresizingMaskIntoConstraints = NO;
    [titlebar addSubview:ui.titleStack];
    [NSLayoutConstraint activateConstraints:@[
        [ui.titleStack.centerXAnchor
            constraintEqualToAnchor:titlebar.centerXAnchor],
        [ui.titleStack.centerYAnchor
            constraintEqualToAnchor:titlebar.centerYAnchor constant:-.5],
        [ui.titleStack.leadingAnchor constraintGreaterThanOrEqualToAnchor:
            closeButton.trailingAnchor constant:12],
        [ui.titleStack.trailingAnchor constraintLessThanOrEqualToAnchor:
            titlebar.trailingAnchor constant:-16],
    ]];

    window.titleVisibility = NSWindowTitleHidden;
    return ui;
}

void
sc_window_ui_init(struct sc_window_ui *ui, SDL_Window *window) {
    ui->window = window;
    ui->data = NULL;
}

void
sc_window_ui_set_device_title(struct sc_window_ui *window_ui,
                              const char *primary,
                              const char *secondary) {
    NSWindow *window = sc_window_ui_get_native_window(window_ui);
    if (!window) {
        return;
    }

    SCScrcpyWindowUI *ui = sc_window_ui_get_or_create_title(window);
    if (!ui.titleStack) {
        return;
    }

    ui.primaryLabel.stringValue = primary
        ? [NSString stringWithUTF8String:primary] : @"";
    bool hasSecondary = secondary && *secondary;
    ui.secondaryLabel.hidden = !hasSecondary;
    ui.secondaryLabel.stringValue = hasSecondary
        ? [NSString stringWithUTF8String:secondary] : @"";
}

static NSPanel *
sc_window_ui_create_toast_panel(NSString *message) {
    NSPanel *panel = [[NSPanel alloc]
        initWithContentRect:NSMakeRect(0, 0, 178, 38)
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    panel.opaque = NO;
    panel.backgroundColor = NSColor.clearColor;
    panel.hasShadow = YES;
    panel.ignoresMouseEvents = YES;
    panel.hidesOnDeactivate = NO;
    panel.collectionBehavior = NSWindowCollectionBehaviorTransient
                             | NSWindowCollectionBehaviorFullScreenAuxiliary;

    NSView *background =
        [[NSView alloc] initWithFrame:panel.contentView.bounds];
    background.wantsLayer = YES;
    background.layer.backgroundColor =
        [NSColor colorWithWhite:.08 alpha:.88].CGColor;
    background.layer.cornerRadius = 12;
    panel.contentView = background;

    NSTextField *label = [NSTextField labelWithString:message];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    label.textColor = NSColor.whiteColor;
    label.alignment = NSTextAlignmentCenter;
    [background addSubview:label];
    [NSLayoutConstraint activateConstraints:@[
        [label.centerXAnchor constraintEqualToAnchor:background.centerXAnchor],
        [label.centerYAnchor constraintEqualToAnchor:background.centerYAnchor],
        [label.leadingAnchor constraintGreaterThanOrEqualToAnchor:
            background.leadingAnchor constant:14],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:
            background.trailingAnchor constant:-14],
    ]];
    return panel;
}

void
sc_window_ui_show_toast(struct sc_window_ui *window_ui, const char *text) {
    NSWindow *window = sc_window_ui_get_native_window(window_ui);
    if (!window || !text) {
        return;
    }

    SCScrcpyWindowUI *ui = sc_window_ui_get_controller(window);

    if (ui.toastPanel) {
        [window removeChildWindow:ui.toastPanel];
        [ui.toastPanel orderOut:nil];
    }
    NSString *message = [NSString stringWithUTF8String:text];
    ui.toastPanel = sc_window_ui_create_toast_panel(message);
    [window addChildWindow:ui.toastPanel ordered:NSWindowAbove];

    NSRect parentFrame = window.frame;
    NSRect toastFrame = ui.toastPanel.frame;
    toastFrame.origin.x = NSMidX(parentFrame) - NSWidth(toastFrame) / 2;
    toastFrame.origin.y = NSMinY(parentFrame) + 58;
    [ui.toastPanel setFrame:toastFrame display:NO];
    ui.toastPanel.alphaValue = 0;
    [ui.toastPanel orderFront:nil];
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = .12;
        ui.toastPanel.animator.alphaValue = 1;
    } completionHandler:nil];

    NSUInteger generation = ++ui.toastGeneration;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t) (2 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        if (ui.toastGeneration != generation || !ui.toastPanel) {
            return;
        }
        NSPanel *panel = ui.toastPanel;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
            context.duration = .18;
            panel.animator.alphaValue = 0;
        } completionHandler:^{
            if (ui.toastGeneration == generation) {
                [window removeChildWindow:panel];
                [panel orderOut:nil];
                ui.toastPanel = nil;
            }
        }];
    });
}

static NSMenu *
sc_window_ui_get_or_create_view_menu(SCScrcpyWindowUI *ui) {
    NSMenu *mainMenu = NSApp.mainMenu;
    if (!mainMenu) {
        return nil;
    }

    for (NSMenuItem *item in mainMenu.itemArray) {
        if ([item.title isEqualToString:@"View"]
                || [item.submenu.title isEqualToString:@"View"]) {
            ui.toolbarMenuContainer = item;
            return item.submenu;
        }
    }

    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    NSMenuItem *viewItem = [[NSMenuItem alloc] initWithTitle:@"View"
                                                     action:nil
                                              keyEquivalent:@""];
    viewItem.submenu = viewMenu;

    NSInteger insertionIndex = mainMenu.numberOfItems;
    if (NSApp.windowsMenu) {
        for (NSInteger i = 0; i < mainMenu.numberOfItems; ++i) {
            if ([mainMenu itemAtIndex:i].submenu == NSApp.windowsMenu) {
                insertionIndex = i;
                break;
            }
        }
    }
    [mainMenu insertItem:viewItem atIndex:insertionIndex];
    ui.toolbarMenuContainer = viewItem;
    ui.ownsToolbarMenuContainer = YES;
    return viewMenu;
}

void
sc_window_ui_set_toolbar_menu_visible(struct sc_window_ui *window_ui,
                                      bool visible) {
    NSWindow *window = sc_window_ui_get_native_window(window_ui);
    if (!window) {
        return;
    }

    SCScrcpyWindowUI *ui = objc_getAssociatedObject(window, SC_WINDOW_UI_KEY);
    if (!ui.toolbarMenuItem) {
        return;
    }
    ui.toolbarMenuItem.title = visible ? @"Hide Toolbar" : @"Show Toolbar";
}

void
sc_window_ui_configure_toolbar_menu(struct sc_window_ui *window_ui,
                                    bool visible,
                                    bool shortcut_enabled,
                                    sc_window_ui_toolbar_toggle_cb on_toggle,
                                    void *userdata) {
    NSWindow *window = sc_window_ui_get_native_window(window_ui);
    if (!window) {
        return;
    }

    SCScrcpyWindowUI *ui = sc_window_ui_get_controller(window);
    ui.toolbarToggleCallback = on_toggle;
    ui.toolbarToggleUserdata = userdata;

    if (!ui.toolbarMenuItem) {
        NSMenu *viewMenu = sc_window_ui_get_or_create_view_menu(ui);
        if (!viewMenu) {
            return;
        }
        ui.toolbarMenuItem = [[NSMenuItem alloc]
            initWithTitle:@""
                   action:@selector(toggleToolbar:)
            keyEquivalent:@""];
        ui.toolbarMenuItem.target = ui;
        [viewMenu addItem:ui.toolbarMenuItem];
    }
    if (shortcut_enabled) {
        ui.toolbarMenuItem.keyEquivalent = @"t";
        ui.toolbarMenuItem.keyEquivalentModifierMask =
            NSEventModifierFlagCommand | NSEventModifierFlagShift;
    } else {
        ui.toolbarMenuItem.keyEquivalent = @"";
    }
    sc_window_ui_set_toolbar_menu_visible(window_ui, visible);
}

bool
sc_window_ui_handle_event(struct sc_window_ui *window_ui,
                          const SDL_Event *event) {
    (void) window_ui;
    (void) event;
    // Native menu commands are dispatched by AppKit.
    return false;
}

void
sc_window_ui_destroy(struct sc_window_ui *window_ui) {
    NSWindow *window = sc_window_ui_get_native_window(window_ui);
    if (!window) {
        return;
    }
    SCScrcpyWindowUI *ui = objc_getAssociatedObject(window, SC_WINDOW_UI_KEY);
    if (ui.toastPanel) {
        [window removeChildWindow:ui.toastPanel];
        [ui.toastPanel orderOut:nil];
    }
    ui.toolbarToggleCallback = NULL;
    ui.toolbarToggleUserdata = NULL;
    [ui.toolbarMenuItem.menu removeItem:ui.toolbarMenuItem];
    if (ui.ownsToolbarMenuContainer
            && ui.toolbarMenuContainer.submenu.numberOfItems == 0) {
        [NSApp.mainMenu removeItem:ui.toolbarMenuContainer];
    }
    [ui.titleStack removeFromSuperview];
    objc_setAssociatedObject(window, SC_WINDOW_UI_KEY, nil,
                             OBJC_ASSOCIATION_ASSIGN);
    window_ui->window = NULL;
    window_ui->data = NULL;
}
