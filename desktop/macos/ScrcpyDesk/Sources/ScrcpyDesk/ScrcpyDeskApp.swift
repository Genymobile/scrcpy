import AppKit
import SwiftUI

@main
struct ScrcpyDeskApp: App {
    init() {
        // Set the Dock/app-switcher icon explicitly. Finder still reads the
        // ICNS declared in Info.plist, while this avoids stale LaunchServices
        // icon caches for an already-running development build.
        if let url = Bundle.main.url(
            forResource: "ScrcpyDeskAppIcon",
            withExtension: "png"
        ), let image = NSImage(contentsOf: url) {
            NSApplication.shared.applicationIconImage = image
        }
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .frame(minWidth: 920, minHeight: 620)
        }
        .defaultSize(width: 1180, height: 760)
        .commands {
            CommandGroup(replacing: .newItem) { }
        }
    }
}
