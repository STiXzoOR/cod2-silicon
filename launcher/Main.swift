import SwiftUI
import AppKit

@MainActor enum LauncherState {
    static let model = LauncherModel(snapshot: CommandLine.arguments.contains("--screens"))
}
@MainActor final class LauncherDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        LauncherState.model.boot()
        NSApp.activate(ignoringOtherApps: true)
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
    init() {
        if let index = CommandLine.arguments.firstIndex(of: "--screens"), CommandLine.arguments.count > index + 1 {
            do { try LauncherSnapshots.render(to: URL(fileURLWithPath: CommandLine.arguments[index + 1])); exit(0) }
            catch { fputs("Screen rendering failed: \(error.localizedDescription)\n", stderr); exit(1) }
        }
    }
    var body: some Scene {
        Window("CoD2 Silicon", id: "launcher") {
            LauncherRoot(model: LauncherState.model).frame(idealWidth: 1440, idealHeight: 900)
        }.defaultSize(width: 1440, height: 900)
            .commands {
                CommandGroup(after: .newItem) {
                    Button("Play") { LauncherState.model.play() }.keyboardShortcut(.return, modifiers: .command)
                    Button("Servers") { LauncherState.model.page = .servers }.keyboardShortcut("2", modifiers: .command)
                    Button("Settings") { LauncherState.model.page = .settings }.keyboardShortcut(",", modifiers: .command)
                }
            }
    }
}

@MainActor enum LauncherSnapshots {
    static func render(to folder: URL) throws {
        _ = NSApplication.shared
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let screens = ["onboarding-data", "onboarding-key", "onboarding-shaders", "home", "servers", "settings", "demos", "screenshots", "about"]
        for dark in [false, true] {
            let appearance = NSAppearance(named: dark ? .darkAqua : .aqua)!
            NSApp.appearance = appearance
            for screen in screens {
                let model = LauncherModel(snapshot: true)
                switch screen {
                case "onboarding-data": model.onboard = true; model.step = 0
                case "onboarding-key": model.onboard = true; model.step = 1
                case "onboarding-shaders": model.onboard = true; model.step = 2; model.shaderProgress = 0.62; model.shaderBusy = true; model.shaderMessage = "Extracting and verifying 834 shader payloads…"
                case "servers": model.page = .servers
                case "settings": model.page = .settings
                case "demos": model.page = .media
                case "screenshots": model.page = .media; model.mediaTab = "Screenshots"; model.media = ["toujane-evening.jpg", "carentan-rooftops.jpg", "a-round-to-remember.jpg"].map { model.home.appendingPathComponent($0) }
                case "about": model.page = .about
                default: break
                }
                let view = NSHostingView(rootView: LauncherRoot(model: model).environment(\.colorScheme, dark ? .dark : .light).frame(width: 1440, height: 900))
                let window = NSWindow(contentRect: NSRect(x: -4000, y: -4000, width: 1440, height: 900), styleMask: [.borderless], backing: .buffered, defer: false)
                window.appearance = appearance; window.contentView = view; window.isReleasedWhenClosed = false
                window.orderFrontRegardless()
                view.layoutSubtreeIfNeeded()
                RunLoop.main.run(until: Date(timeIntervalSinceNow: 0.15))
                guard let bitmap = view.bitmapImageRepForCachingDisplay(in: view.bounds) else { throw LauncherError(message: "Cannot allocate snapshot bitmap.") }
                view.cacheDisplay(in: view.bounds, to: bitmap)
                guard let png = bitmap.representation(using: .png, properties: [:]) else { throw LauncherError(message: "Cannot encode snapshot.") }
                try png.write(to: folder.appendingPathComponent("\(screen)-\(dark ? "dark" : "light").png"))
                window.orderOut(nil); window.close()
            }
        }
        print("Rendered 18 launcher screens at 1440×900 to \(folder.path)")
    }
}
