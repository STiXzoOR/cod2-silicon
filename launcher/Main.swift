import SwiftUI
import AppKit

@MainActor enum LauncherState {
    static let model = LauncherModel()
    static var reopen: (() -> Void)?
    static func registerWindowAction(_ openWindow: OpenWindowAction) {
        reopen = { openWindow(id: "launcher") }
    }
    static func showWindow() {
        NSApp.setActivationPolicy(.regular)
        NSApp.unhide(nil)
        reopen?()
        let window = model.launcherWindow ?? NSApp.windows.first { $0.canBecomeMain }
        window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}
@MainActor final class LauncherDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
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
            LauncherState.showWindow()
            LauncherState.model.notice = "Call of Duty 2 is still running. Quit the game first, then quit CoD2 Silicon."
            return .terminateCancel
        }
        return .terminateNow
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if LauncherState.model.gameRunning { LauncherState.model.returnToGame() }
        else { LauncherState.showWindow() }
        return false
    }
}

@main struct CoD2LauncherApp: App {
    @NSApplicationDelegateAdaptor(LauncherDelegate.self) var delegate
    init() {
        // Engine arguments after `--` are not documents for AppKit to open.
        UserDefaults.standard.register(defaults: ["NSTreatUnknownArgumentsAsOpen": false])
        LauncherFonts.register()
    }
    var body: some Scene {
        Window("CoD2 Silicon", id: "launcher") {
            LauncherScene().frame(idealWidth: 1440, idealHeight: 900)
        }
        .defaultSize(width: 1440, height: 900)
        .windowStyle(.hiddenTitleBar)
        .windowToolbarStyle(.unified)
        .commands { LauncherCommands(model: LauncherState.model) }
    }
}

private struct LauncherScene: View {
    @Environment(\.openWindow) private var openWindow
    var body: some View {
        LauncherRoot(model: LauncherState.model)
            .onAppear {
                LauncherState.registerWindowAction(openWindow)
            }
    }
}

struct LauncherCommands: Commands {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var discover: ConfigDiscoverModel
    @Environment(\.openWindow) private var openWindow
    init(model: LauncherModel) { self.model = model; discover = model.discover }
    private func show(_ page: LauncherPage) {
        openWindow(id: "launcher")
        model.navigate(to: page)
        NSApp.activate(ignoringOtherApps: true)
    }
    var body: some Commands {
        // URL launches may never create a view. Commands still supply the scene-opening action.
        let _ = LauncherState.registerWindowAction(openWindow)
        CommandGroup(replacing: .appInfo) {
            Button("About CoD2 Silicon") { show(.about) }.disabled(!model.canNavigate)
            Button("Check for Updates…") { show(.about); model.checkUpdates() }
                .disabled(!model.canNavigate || model.checkingUpdates || model.installingUpdate)
        }
        CommandGroup(replacing: .appSettings) {
            Button("Settings…") { show(.settings) }.keyboardShortcut(",", modifiers: .command).disabled(!model.canNavigate)
        }
        CommandGroup(replacing: .newItem) {
            Button("Deploy") { show(.deploy); model.deploy() }.keyboardShortcut(.return, modifiers: .command).disabled(!model.canNavigate)
            Button("Open Game Menu") { show(.deploy); model.play() }.keyboardShortcut(.return, modifiers: [.command, .shift]).disabled(!model.canNavigate)
            Divider()
            Button("Import a Config…") { show(.configs); discover.chooseFile() }
                .keyboardShortcut("o", modifiers: .command).disabled(!model.canImport)
            Button("Review Pasted Config") { show(.configs); discover.importPasteboard() }
                .keyboardShortcut("v", modifiers: [.command, .shift]).disabled(!model.canImport)
            Divider()
            Button("Show CoD2 Silicon Folder in Finder") { model.openHomeFolder() }.disabled(!model.canNavigate)
        }
        SidebarCommands()
        // SwiftUI can leave the native item's title stale when visibility is bound.
        // Keep the standard menu position and shortcut, driven by the same binding.
        CommandGroup(replacing: .sidebar) {
            Button(model.sidebarVisibility == .detailOnly ? "Show Sidebar" : "Hide Sidebar") {
                model.sidebarVisibility = model.sidebarVisibility == .detailOnly ? .all : .detailOnly
            }
            .keyboardShortcut("s", modifiers: [.command, .control])
            .disabled(!model.canNavigate || model.launcherWindow?.isVisible != true)
        }
        CommandGroup(before: .sidebar) {
            Toggle("Deploy", isOn: selection(.deploy)).keyboardShortcut("1", modifiers: .command).disabled(!model.canNavigate)
            Toggle("Servers", isOn: selection(.servers)).keyboardShortcut("2", modifiers: .command).disabled(!model.canNavigate)
            Toggle("Library", isOn: selection(.library)).keyboardShortcut("3", modifiers: .command).disabled(!model.canNavigate)
            Toggle("Configs", isOn: selection(.configs)).keyboardShortcut("4", modifiers: .command).disabled(!model.canNavigate)
            Divider()
            Button("Refresh Server List") { model.refresh() }.keyboardShortcut("r", modifiers: .command)
                .disabled(!model.canNavigate || model.page != .servers || model.refreshing)
            Divider()
        }
        CommandGroup(after: .windowList) {
            Button("CoD2 Silicon") { openWindow(id: "launcher"); NSApp.activate(ignoringOtherApps: true) }
                .disabled(model.gameRunning)
        }
        CommandGroup(replacing: .help) {
            Button("CoD2 Silicon Help") { openHelp("") }
            Button("Where to Find Your Game Data") { openHelp("/blob/main/docs/macos-port/game-data.md") }
            Button("Release Notes") { openHelp("/releases") }
            Button("Report a Problem…") { openHelp("/issues") }
        }
    }
    private func selection(_ page: LauncherPage) -> Binding<Bool> {
        Binding(get: { !model.onboard && model.page == page }, set: { _ in show(page) })
    }
    private func openHelp(_ suffix: String) {
        NSWorkspace.shared.open(URL(string: "https://github.com/STiXzoOR/cod2-silicon" + suffix)!)
    }
}
