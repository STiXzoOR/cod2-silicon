import SwiftUI
import AppKit

@MainActor enum LauncherState {
    static let model = LauncherModel()
}
@MainActor final class LauncherDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Become a regular, active app first; boot() may start the game straight away (a cold
        // cod2x:// link or --play) and then hands the Dock to it.
        if !LauncherState.model.gameRunning {
            NSApp.setActivationPolicy(.regular)
            NSApp.activate(ignoringOtherApps: true)
        }
        LauncherState.model.boot()
    }
    func application(_ application: NSApplication, open urls: [URL]) {
        for url in urls { LauncherState.model.openLink(url.absoluteString) }
    }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        if LauncherState.model.gameRunning {
            LauncherState.model.notice = "Quit the game first, then quit CoD2 Silicon."
            return .terminateCancel
        }
        return .terminateNow
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if !LauncherState.model.gameRunning { sender.windows.first?.makeKeyAndOrderFront(nil) }
        return true
    }
}

@main struct CoD2LauncherApp: App {
    @NSApplicationDelegateAdaptor(LauncherDelegate.self) var delegate
    init() { LauncherFonts.register() }
    var body: some Scene {
        Window("CoD2 Silicon", id: "launcher") {
            LauncherRoot(model: LauncherState.model).frame(idealWidth: 1440, idealHeight: 900)
        }
        .defaultSize(width: 1440, height: 900)
        .windowStyle(.hiddenTitleBar)
        .windowToolbarStyle(.unified)
        .commands {
            CommandGroup(replacing: .appInfo) {
                // Setup must finish first: About never bypasses onboarding.
                Button("About CoD2 Silicon") { if !LauncherState.model.onboard { LauncherState.model.page = .about } }
            }
            CommandGroup(after: .newItem) {
                Button("Deploy") { LauncherState.model.deploy() }.keyboardShortcut(.return, modifiers: .command)
                Button("Open Game Menu") { LauncherState.model.play() }.keyboardShortcut(.return, modifiers: [.command, .shift])
                Divider()
                Button("Servers") { LauncherState.model.page = .servers }.keyboardShortcut("2", modifiers: .command)
                Button("Library") { LauncherState.model.page = .library; LauncherState.model.loadMedia() }.keyboardShortcut("3", modifiers: .command)
                Button("Settings") { LauncherState.model.page = .settings }.keyboardShortcut(",", modifiers: .command)
            }
        }
    }
}
