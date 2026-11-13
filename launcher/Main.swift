import SwiftUI
import AppKit
import UserNotifications

@MainActor enum LauncherState {
    static let model = LauncherModel()
    static var reopen: (() -> Void)?
    static func registerWindowAction(_ openWindow: OpenWindowAction) {
        reopen = { openWindow(id: LauncherWindowID.main.rawValue) }
        // The model opens either window (Settings from a Home chip, the main window for setup).
        model.openWindowHandler = { openWindow(id: $0.rawValue) }
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
@MainActor final class LauncherDelegate: NSObject, NSApplicationDelegate, UNUserNotificationCenterDelegate {
    private let dockTarget = DockMenuTarget()
    func applicationDidFinishLaunching(_ notification: Notification) {
        if !LauncherState.model.gameRunning {
            NSApp.setActivationPolicy(.regular)
            NSApp.activate(ignoringOtherApps: true)
        }
        // Only the delegate is set here; permission is asked when the first notification would be useful.
        if ShellNotifications.available { UNUserNotificationCenter.current().delegate = self }
        LauncherState.model.boot()
        if let path = ProcessInfo.processInfo.environment["COD2_DUMP_MENUS"] { MenuDump.schedule(to: path) }
    }
    /// Join links, demos and configs from Finder, a browser or the Dock icon (ModelShell.swift).
    func application(_ application: NSApplication, open urls: [URL]) { LauncherState.model.open(urls) }
    /// Quitting with the game open asks whether to leave the game running or quit both.
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        let model = LauncherState.model
        if model.gameRunning && !model.quitConfirmed {
            LauncherState.showWindow()
            model.confirmQuitWithGame()
            return .terminateCancel
        }
        return .terminateNow
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    /// Clicking the Dock icon brings the launcher back, also while the game runs (Surfaces board).
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        LauncherState.showWindow()
        return false
    }
    func applicationDockMenu(_ sender: NSApplication) -> NSMenu? { LauncherState.model.dockMenu(target: dockTarget) }
    nonisolated func userNotificationCenter(_ center: UNUserNotificationCenter, didReceive response: UNNotificationResponse) async {
        let kind = (response.notification.request.content.userInfo["kind"] as? String).flatMap(ShellNotifications.Kind.init(rawValue:))
        await MainActor.run {
            LauncherState.showWindow()
            if let kind { LauncherState.model.openNotification(kind) }
        }
    }
    nonisolated func userNotificationCenter(_ center: UNUserNotificationCenter, willPresent notification: UNNotification) async -> UNNotificationPresentationOptions {
        [.banner, .list]
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
        Window("CoD2 Silicon", id: LauncherWindowID.main.rawValue) {
            LauncherScene().frame(idealWidth: 1440, idealHeight: 900)
        }
        .defaultSize(width: 1440, height: 900)
        .windowStyle(.hiddenTitleBar)
        .windowToolbarStyle(.unified)
        .commands { LauncherCommands(model: LauncherState.model) }
        // Settings is its own window (SettingsWin board); `model.openSettings(_:)` picks the pane.
        Window("Settings", id: LauncherWindowID.settings.rawValue) {
            SettingsScene()
        }
        .defaultSize(width: SettingsWindowMetrics.width, height: SettingsWindowMetrics.height)
        .windowResizability(.contentSize)
        .defaultPosition(.center)
    }
}

private struct SettingsScene: View {
    @Environment(\.openWindow) private var openWindow
    var body: some View {
        SettingsWindowRoot(model: LauncherState.model)
            .onAppear { LauncherState.registerWindowAction(openWindow) }
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

/// The menu bar, as the Menus board: every item in its place, dimmed (never hidden) when it can't run.
struct LauncherCommands: Commands {
    @ObservedObject var model: LauncherModel
    @ObservedObject private var discover: ConfigDiscoverModel
    @ObservedObject private var studio: ConfigStudioModel
    @Environment(\.openWindow) private var openWindow
    init(model: LauncherModel) { self.model = model; discover = model.discover; studio = model.studio }
    private func show(_ page: LauncherPage) {
        openWindow(id: LauncherWindowID.main.rawValue)
        model.navigate(to: page)
        NSApp.activate(ignoringOtherApps: true)
    }
    /// Settings and its panes stay reachable while the game runs; not during setup or an update install.
    private var settingsAvailable: Bool { !model.onboard && !model.installingUpdate }
    /// The Server menu's row commands act on the selection Servers is showing.
    private var selectedServer: GameServer? { model.page == .servers && model.canNavigate ? model.selected : nil }
    var body: some Commands {
        // URL launches may never create a view. Commands still supply the scene-opening action.
        let _ = LauncherState.registerWindowAction(openWindow)
        CommandGroup(replacing: .appInfo) {
            Button("About CoD2 Silicon") { model.openSettings(.about) }.disabled(!settingsAvailable)
        }
        CommandGroup(replacing: .appSettings) {
            Button("Check for Updates…") { model.openSettings(.updates); model.checkUpdates() }
                .disabled(!settingsAvailable || model.checkingUpdates)
            Button("Settings…") { model.openSettings() }.keyboardShortcut(",", modifiers: .command).disabled(!settingsAvailable)
        }
        fileMenu
        CommandGroup(replacing: .saveItem) {
            Button("Close Window") { NSApp.keyWindow?.performClose(nil) }.keyboardShortcut("w", modifiers: .command)
        }
        CommandGroup(replacing: .textEditing) {
            Button("Find") { find() }.keyboardShortcut("f", modifiers: .command)
                .disabled(!model.canNavigate || !(model.page == .servers || model.page == .configs))
        }
        viewMenu
        CommandMenu("Server") { serverMenu }
        CommandMenu("Loadout") { loadoutMenu }
        CommandGroup(replacing: .help) {
            Button("CoD2 Silicon Help") { model.openHelp() }.keyboardShortcut("?", modifiers: .command)
            Button("Keyboard Shortcuts") { model.showMainWindow(); model.showKeyboardShortcuts() }
            Divider()
            Button("Release Notes") { model.openReleaseNotes() }
            Button("Report a Problem…") { model.reportProblem() }
            Divider()
            Button("Show Logs in Finder") { model.showLogsInFinder() }
        }
    }
    @CommandsBuilder private var fileMenu: some Commands {
        CommandGroup(replacing: .newItem) {
            Button(model.launchMenuTitle) { model.showMainWindow(); model.launchFromMenu() }
                .keyboardShortcut(.return, modifiers: .command).disabled(!model.canNavigate)
            Button("Open Game Menu") { show(.home); model.openGameMenu() }
                .keyboardShortcut(.return, modifiers: [.command, .shift]).disabled(!model.canNavigate)
            Divider()
            Button("Connect to Address…") { model.showMainWindow(); model.connectToAddress() }
                .keyboardShortcut("l", modifiers: .command).disabled(model.onboard || model.installingUpdate)
            Divider()
            Button("Import Config…") { model.importConfig() }.keyboardShortcut("o", modifiers: .command).disabled(!model.canImport)
            Button("Paste Config") { model.pasteConfig() }.keyboardShortcut("v", modifiers: [.command, .shift]).disabled(!model.canImport)
            Button("Export Loadout…") { model.showMainWindow(); model.exportLoadout() }
                .keyboardShortcut("e", modifiers: [.command, .shift]).disabled(!model.canNavigate || studio.profiles.isEmpty)
            Divider()
            Button("Show Game Data in Finder") { model.showGameDataInFinder() }.disabled(model.dataPath.isEmpty)
            Button("Show My Files in Finder") { model.showMyFilesInFinder() }
        }
    }
    @CommandsBuilder private var viewMenu: some Commands {
        SidebarCommands()
        // SwiftUI can leave the native item's title stale when visibility is bound.
        // Keep the standard menu position and shortcut, driven by the same binding.
        CommandGroup(replacing: .sidebar) {
            Button(model.sidebarVisibility == .detailOnly ? "Show Sidebar" : "Hide Sidebar") { model.toggleSidebar() }
                .keyboardShortcut("s", modifiers: [.command, .control])
                .disabled(model.onboard || model.launcherWindow?.isVisible != true)
        }
        CommandGroup(before: .sidebar) {
            ForEach(LauncherPage.navigation) { page in
                Toggle(page.title, isOn: selection(page)).keyboardShortcut(KeyEquivalent(page.shortcut), modifiers: .command).disabled(!model.canNavigate)
            }
            Divider()
            Button("Jump to…") { model.showMainWindow(); model.showJump() }.keyboardShortcut("k", modifiers: .command).disabled(!model.canNavigate)
            Divider()
        }
    }
    @ViewBuilder private var serverMenu: some View {
        Button("Refresh Server List") { show(.servers); model.refresh() }.keyboardShortcut("r", modifiers: .command)
            .disabled(!model.canNavigate || model.refreshing)
        Button("Filters…") { show(.servers); model.showServerFilters() }.keyboardShortcut("f", modifiers: [.command, .option]).disabled(!model.canNavigate)
        Divider()
        // ⌘↩ belongs to File › Launch, which also launches on this selection.
        Button("Launch on Selected Server") { model.launchSelectedServer() }.disabled(selectedServer == nil)
        let favorite = selectedServer.map { model.library.favorites.contains($0.address) } ?? false
        Button(favorite ? "Remove from Favorites…" : "Add to Favorites") {
            if favorite, let server = selectedServer { model.requestRemoveFavorite(server) } else { model.toggleFavoriteSelected() }
        }
        .keyboardShortcut("d", modifiers: .command).disabled(selectedServer == nil)
        Button("Copy Address") { model.copySelectedAddress() }.keyboardShortcut("c", modifiers: [.command, .shift]).disabled(selectedServer == nil)
        Divider()
        Menu("Recent Servers") {
            ForEach(model.recentServers) { server in
                Button(QuakeColors.plain(server.name)) { model.connect(server) }
            }
        }
        .disabled(model.recentServers.isEmpty || !model.canNavigate)
        Button("Clear Recent Servers") { model.clearRecentServers() }.disabled(model.library.recent.isEmpty || !model.canNavigate)
    }
    @ViewBuilder private var loadoutMenu: some View {
        let editable = model.canNavigate && !studio.profiles.isEmpty
        Menu("Use for Next Game") {
            ForEach(studio.profiles) { profile in
                Toggle(profile.displayName, isOn: Binding(get: { profile.isActive }, set: { _ in model.useLoadoutForNextGame(profile.id) }))
            }
        }
        .disabled(!editable)
        Divider()
        Button("New Loadout…") { show(.configs); model.newLoadout() }.keyboardShortcut("n", modifiers: .command).disabled(!editable)
        Button("Duplicate") { show(.configs); model.duplicateLoadout() }.keyboardShortcut("d", modifiers: [.command, .shift]).disabled(!editable)
        Button("Rename…") { show(.configs); model.renameLoadout() }.disabled(!editable)
        Divider()
        Button("Run Check-up") { model.showMainWindow(); model.openCheckup() }.disabled(!model.canNavigate)
        Button("Show History") { model.showMainWindow(); model.showHistory() }.disabled(!editable)
        Divider()
        Button("Delete Loadout…") { show(.configs); model.requestDeleteLoadout() }.disabled(!editable || studio.profiles.count < 2)
    }
    private func selection(_ page: LauncherPage) -> Binding<Bool> {
        Binding(get: { !model.onboard && model.page == page }, set: { _ in show(page) })
    }
    /// Edit › Find: the page's own search (Servers, Configs) takes the same ⌘F the menu shows.
    private func find() {
        model.showMainWindow()
        guard let window = model.launcherWindow,
              let event = NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: .command, timestamp: ProcessInfo.processInfo.systemUptime,
                                           windowNumber: window.windowNumber, context: nil, characters: "f", charactersIgnoringModifiers: "f",
                                           isARepeat: false, keyCode: 3),
              window.performKeyEquivalent(with: event) else { model.findInPage(); return }
    }
}

/// `COD2_DUMP_MENUS=<file>`: writes the menu bar (titles, shortcuts, enabled, ticks) a moment after
/// launch, so the lifecycle checks can compare it with the Menus board without the accessibility API.
@MainActor enum MenuDump {
    static func schedule(to path: String) {
        Task { @MainActor in
            try? await Task.sleep(for: .seconds(2))
            NSApp.mainMenu?.update()
            var lines: [String] = []
            func walk(_ menu: NSMenu, depth: Int) {
                menu.update()
                for item in menu.items {
                    if item.isSeparatorItem { lines.append(String(repeating: "  ", count: depth) + "-"); continue }
                    let modifiers = item.keyEquivalentModifierMask
                    let keys = item.keyEquivalent.isEmpty ? "" : (modifiers.contains(.control) ? "⌃" : "") + (modifiers.contains(.option) ? "⌥" : "")
                        + (modifiers.contains(.shift) ? "⇧" : "") + (modifiers.contains(.command) ? "⌘" : "") + item.keyEquivalent.replacingOccurrences(of: "\r", with: "↩").uppercased()
                    lines.append(String(repeating: "  ", count: depth) + (item.state == .on ? "✓ " : "") + item.title + (keys.isEmpty ? "" : "\t" + keys) + (item.isEnabled ? "" : "\t(dimmed)"))
                    if let submenu = item.submenu, depth < 2 { walk(submenu, depth: depth + 1) }
                }
            }
            if let menu = NSApp.mainMenu { walk(menu, depth: 0) }
            try? lines.joined(separator: "\n").write(toFile: path, atomically: true, encoding: .utf8)
        }
    }
}
