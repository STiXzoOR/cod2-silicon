import AppKit
import SwiftUI
import UserNotifications

// Owner: ship-shell. Everything outside the window and around the game's lifecycle: links and files
// from Finder and the Dock, the Dock menu, notifications, quitting with the game open, the Game
// Couldn't Start sheet's fixes, File › Launch's target, and the player card's name.

/// Why Launch failed, with the one fix the Game Couldn't Start sheet offers.
enum LaunchProblem: Equatable, Identifiable {
    /// The saved game data folder isn't there (moved, or its drive isn't connected).
    case dataMissing(String)
    /// This copy of the app is missing the game itself.
    case helperMissing
    /// Anything else, in the error's own words.
    case other(String)
    var id: String { message }
    var message: String {
        switch self {
        case .dataMissing: "The game data folder has moved or its drive isn’t connected."
        case .helperMissing: "This copy of CoD2 Unbound is missing the game itself. Download CoD2 Unbound again to put it back."
        case .other(let text): text
        }
    }
    /// The path to show under the message, when there is one.
    var path: String? { if case .dataMissing(let path) = self { PathDisplay.short(path) } else { nil } }
    var fixTitle: String {
        switch self { case .dataMissing: "Choose Folder…"; case .helperMissing: "Open Releases Page"; case .other: "Try Again" }
    }
}

/// What a name other players see may hold: the config line it is saved in can't carry quotes or separators.
enum PlayerNameRules {
    static func problem(_ name: String) -> String? {
        GameSettings.safeValue(name) ? nil : "A name can’t contain quotes, semicolons, plus signs or backslashes."
    }
    /// The colour codes the game reads in a name (Add Colours…), as the Quake palette names them.
    static let colours: [(code: Int, name: String)] = [(1, "Red"), (2, "Green"), (3, "Yellow"), (4, "Blue"), (5, "Cyan"), (6, "Pink"), (7, "White"), (0, "Black")]
}

@MainActor extension LauncherModel {
    // MARK: While the game runs
    /// Settings › General › Hide CoD2 Unbound while the game is running.
    var hidesLauncherWhilePlaying: Bool { settings.hideWhilePlaying }
    /// The game has the screen: the launcher keeps its Dock icon (clicking it brings the launcher
    /// back), and its windows step aside if the player wants that.
    func gameTookOver() {
        jumpPresented = false; playerCardPresented = false
        page = .home
        guard hidesLauncherWhilePlaying else { return }
        NSApp.windows.forEach { $0.orderOut(nil) }
        // Reconnecting at launch: the window doesn't exist yet, so it hides when it appears.
        if launcherWindow == nil { hideLauncherWindowOnAppear = true }
    }
    /// Home's Return to Game and the notice's button: the game in front, the launcher out of its way.
    func returnToGame() {
        guard let pid = activeGamePID, let game = NSRunningApplication(processIdentifier: pid) else { return }
        if hidesLauncherWhilePlaying { launcherWindow?.orderOut(nil); settingsWindow?.orderOut(nil) }
        if #available(macOS 14.0, *) {
            NSApp.yieldActivation(to: game)
            game.activate(from: NSRunningApplication.current, options: [.activateAllWindows])
        } else {
            game.activate(options: .activateAllWindows)
        }
    }
    /// Home's Quit Game and Quit Both: asks the game to quit the way its own Quit does; if it hasn't
    /// gone after ten seconds it is stopped. `completion` runs once it has gone.
    func quitGame(then completion: (@MainActor () -> Void)? = nil) {
        guard let pid = activeGamePID, let game = NSRunningApplication(processIdentifier: pid) else { completion?(); return }
        quitGameTask?.cancel()
        notice = "Quitting Call of Duty 2…"
        if !game.terminate() { engine?.terminate() }
        quitGameTask = Task { [weak self] in
            for tick in 0..<80 {
                if game.isTerminated || self?.activeGamePID != pid { break }
                if tick == 40 { _ = game.forceTerminate() }
                try? await Task.sleep(for: .milliseconds(250))
            }
            guard let self else { return }
            if self.notice == "Quitting Call of Duty 2…" { self.notice = "" }
            completion?()
        }
    }
    /// Quitting with the game open asks first (AllSheets › Quit while the game runs).
    func confirmQuitWithGame() {
        confirm(ConfirmationRequest(title: "Call of Duty 2 is still running",
                                    message: "Quitting CoD2 Unbound leaves the game open. Match summaries for this session won’t be saved.",
                                    actionTitle: "Quit Launcher Only", destructive: false,
                                    others: [(title: "Quit Both", action: { [weak self] in
                                        self?.quitConfirmed = true
                                        self?.quitGame { NSApp.terminate(nil) }
                                    })]) { [weak self] in
            self?.quitConfirmed = true
            NSApp.terminate(nil)
        })
    }

    // MARK: Launch and its failures
    /// File › Launch (⌘↩): the selection shown in Servers, otherwise the last server.
    func launchFromMenu() {
        jumpPresented = false
        if page == .servers, let selected { connect(selected); return }
        launch()
    }
    /// The menu item's name for what ⌘↩ does now.
    var launchMenuTitle: String { "Launch" }
    /// A launch that failed before the game started: the sheet says why and offers the fix.
    func launchFailed(_ error: Error) {
        let problem: LaunchProblem
        if !dataPath.isEmpty, LauncherFindData(dataPath) == nil { problem = .dataMissing(dataPath) }
        else if !FileManager.default.isExecutableFile(atPath: chosenEngine.executable(in: gameBundle).path) { problem = .helperMissing }
        else { problem = .other(error.localizedDescription) }
        launchProblem = problem
        showMainWindow()
        present(.gameCouldNotStart(reason: problem.message))
    }
    /// The Game Couldn't Start sheet's default action.
    func fixLaunchProblem() {
        let problem = launchProblem
        dismissSheet(); launchProblem = nil
        switch problem {
        case .dataMissing: chooseGameFolderAndLaunch()
        case .helperMissing: NSWorkspace.shared.open(AppUpdate.releases)
        case .other, nil: play()
        }
    }
    /// Choose Folder… from the sheet: a valid folder is remembered and the game starts; a wrong one says what's missing.
    func chooseGameFolderAndLaunch() {
        guard let window = launcherWindow ?? NSApp.keyWindow else { return }
        let panel = makeSetupPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.treatsFilePackagesAsDirectories = true
        panel.prompt = "Choose Folder"
        panel.message = "Choose your Call of Duty 2 folder: the one that contains “main” with iw_00.iwd through iw_15.iwd."
        let present = { [weak self] (response: NSApplication.ModalResponse) in
            guard let self, response == .OK, let url = panel.url else { return }
            guard let valid = LauncherFindData(url.path) else {
                let survey = GameArchives.survey(url.path)
                self.notice = "That folder has \(survey.found) of the 16 game archives. Choose the Call of Duty 2 folder that contains “main” with iw_00.iwd through iw_15.iwd."
                return
            }
            self.dataPath = valid; self.confirmedDataPath = valid
            do { try self.rememberData() } catch { self.notice = error.localizedDescription; return }
            self.loadArtwork()
            self.play()
        }
        // The sheet is closing; wait for it before attaching the panel.
        Task { try? await Task.sleep(for: .milliseconds(350)); panel.beginSheetModal(for: window, completionHandler: present) }
    }

    // MARK: Links and files from outside
    /// Links, demos and configs opened from Finder, a browser or the Dock icon.
    func open(_ urls: [URL]) {
        for url in urls {
            if url.isFileURL {
                switch url.pathExtension.lowercased() {
                case "dm_1": openDemo(url)
                case "cfg": openConfigFile(url)
                default: break
                }
            } else { receiveLink(url.absoluteString) }
        }
    }
    /// A join link (`cod2x://host:port`, CoD2x's command links, or a pasted `cod2://` one) opens Connect to
    /// Address with the address filled in. It never starts the game by itself.
    func receiveLink(_ text: String) {
        // Unattended runs (the hand-off tests) keep the direct join they automate.
        if ProcessInfo.processInfo.environment["COD2_SETUP_NONINTERACTIVE"] == "1" { openLink(text); return }
        showMainWindow()
        guard let address = Self.joinAddress(text) else {
            notice = "This link isn’t a server address CoD2 Unbound can open."
            return
        }
        print("CoD2 Unbound: join link opened Connect to Address (\(address)).")
        fflush(stdout)
        let input = ServerAddressInput.parse(text)?.password == nil ? address : text
        if onboard { pendingJoinAddress = input; return }
        dismissSheet()
        connectToAddress(input)
    }
    /// The server address in a join link, or nil.
    nonisolated static func joinAddress(_ text: String) -> String? {
        if let link = try? LaunchLink(text) { return link.address }
        for scheme in ["cod2x://", "cod2://"] where text.lowercased().hasPrefix(scheme) {
            var rest = String(text.dropFirst(scheme.count))
            if rest.hasSuffix("/") { rest.removeLast() }
            if rest.lowercased().hasPrefix("connect/") { rest = String(rest.dropFirst(8)) }
            rest = rest.removingPercentEncoding ?? rest
            guard !rest.isEmpty, !rest.contains(" "), (try? LaunchLink.canonicalAddress(rest)) != nil else { return nil }
            return rest
        }
        return nil
    }
    /// A demo double-clicked in Finder: it joins the demos folder (if it isn't there already) and plays.
    func openDemo(_ url: URL) {
        showMainWindow()
        guard !onboard else { notice = "Finish setup, then open the recording again."; return }
        guard !gameRunning else { notice = "Close the game before playing a recording."; return }
        let folder = home.appendingPathComponent("main/demos")
        var target = folder.appendingPathComponent(url.lastPathComponent)
        if url.standardizedFileURL.deletingLastPathComponent().path != folder.standardizedFileURL.path {
            do {
                try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
                let base = url.deletingPathExtension().lastPathComponent
                var index = 2
                while FileManager.default.fileExists(atPath: target.path) {
                    if FileManager.default.contentsEqual(atPath: target.path, andPath: url.path) { break }
                    target = folder.appendingPathComponent("\(base)-\(index).dm_1"); index += 1
                }
                if !FileManager.default.fileExists(atPath: target.path) { try FileManager.default.copyItem(at: url, to: target) }
            } catch { notice = "“\(url.lastPathComponent)” couldn’t be added to your demos. \(error.localizedDescription)"; return }
        }
        navigate(to: .library)
        play(demo: target)
    }
    /// A config opened or dropped on the Dock icon or the window: import review opens. Nothing changes until it is applied.
    func openConfigFile(_ url: URL) {
        showMainWindow()
        guard canImport else {
            notice = onboard ? "Finish setup, then open the config again." : gameRunning ? "Close the game, then open the config again." : "Finish the import that is open, then open this config."
            return
        }
        navigate(to: .configs)
        discover.importFile(url)
    }

    // MARK: Dock menu
    func dockMenu(target: DockMenuTarget) -> NSMenu {
        let menu = NSMenu()
        let recent = recentServers.prefix(5)
        if !recent.isEmpty {
            menu.addItem(NSMenuItem.sectionHeader(title: "Recent servers"))
            for server in recent {
                menu.addItem(target.item(QuakeColors.plain(server.name), enabled: canNavigate) { [weak self] in self?.connect(server) })
            }
            menu.addItem(.separator())
        }
        menu.addItem(target.item("Launch on Last Server", enabled: canNavigate && lastServer != nil) { [weak self] in self?.launch() })
        menu.addItem(target.item("Open Game Menu", enabled: canNavigate) { [weak self] in self?.openGameMenu() })
        menu.addItem(target.item("Connect to Address…", enabled: !onboard) { [weak self] in self?.showMainWindow(); self?.connectToAddress() })
        return menu
    }

    // MARK: Notifications
    /// Setup built the shaders while the launcher was in the background.
    func notifyShadersReady() {
        ShellNotifications.post(id: "shaders-ready", title: "Original shaders are ready", body: "Setup can continue.", kind: .shaders)
    }
    /// An automatic update check found a newer release (ship-settings calls this). Never while the game runs.
    func notifyUpdateReady(version: String, installsOnQuit: Bool) {
        guard !gameRunning, !snapshot else { return }
        ShellNotifications.post(id: "update-\(version)", title: "CoD2 Unbound \(version) is ready",
                                body: installsOnQuit ? "Installs the next time you quit. Click to see what’s new." : "Click to see what’s new.", kind: .update)
    }
    /// A notification was clicked.
    func openNotification(_ kind: ShellNotifications.Kind) {
        showMainWindow()
        if kind == .update { showWhatsNew() }
    }

    // MARK: Player card
    /// The loadout the name is saved in: the one the next game uses.
    var playerLoadoutName: String? { nextGameLoadout?.displayName }
    /// The sidebar plate's second line: the next game's loadout.
    var playerPlateDetail: String {
        if let name = nextGameLoadoutName ?? playerLoadoutName { return "\(name) loadout" }
        return keyVerified ? "CD key verified" : "CD key needed"
    }
    /// Saves as the person types, after a short pause (one History entry per pause, not per key).
    func schedulePlayerNameSave(_ name: String) {
        playerNameSave?.cancel()
        if let problem = PlayerNameRules.problem(name) { playerCardStatus = nil; playerCardError = problem; return }
        playerCardError = nil
        // The saved name itself (the card opening, or typing back to it) keeps the last status.
        guard name != playerName else { playerNameSave = nil; return }
        playerCardStatus = nil
        guard !name.trimmingCharacters(in: .whitespaces).isEmpty else { playerNameSave = nil; return }
        playerNameSave = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(700))
            guard let self, !Task.isCancelled else { return }
            self.commitPlayerName(name)
        }
    }
    /// The card closes: anything still waiting is saved now.
    func flushPlayerName(_ name: String) {
        guard playerNameSave != nil else { return }
        playerNameSave?.cancel(); playerNameSave = nil
        guard PlayerNameRules.problem(name) == nil, !name.trimmingCharacters(in: .whitespaces).isEmpty else { return }
        commitPlayerName(name)
    }
    private func commitPlayerName(_ name: String) {
        playerNameSave = nil
        guard name != playerName else { return }
        // Review screens without loadouts keep the name in memory; with the harness's loadouts it saves for real.
        if snapshot && studio.session == nil { playerName = name; playerCardStatus = "Saved to your \(playerLoadoutName ?? "Main") loadout."; return }
        if let error = savePlayerNameNow(name) { playerCardError = error; return }
        playerCardStatus = "Saved to your \(playerLoadoutName ?? "Main") loadout."
        studio.announce(playerCardStatus ?? "")
    }
    /// Writes `name` into the loadout the next game uses, through Config Studio (so History has it
    /// and it can be undone there). Returns what went wrong, or nil.
    func savePlayerNameNow(_ name: String) -> String? {
        guard studio.session != nil else { return "Your name can’t be saved until your loadouts are ready. Try again after the first game." }
        if studio.isReadOnly { return "The game is running. Change your name after you quit it." }
        if studio.catalog == nil { studio.loadCatalogNow() }
        guard let spec = studio.spec(named: "name"), let active = studio.activeProfile else {
            return "Your name can’t be saved until your loadouts are ready. Try again after the first game."
        }
        let focus = studio.current?.profile.id
        let other = focus.map { $0.caseInsensitiveCompare(active.id) != .orderedSame } ?? true
        if other { studio.select(active.id) }
        let before = studio.failure?.title
        studio.set(spec, to: name, summary: "Changed your name")
        let failed = studio.failure.flatMap { $0.title != before ? $0.message : nil }
        if other, let focus { studio.select(focus) }
        if let failed { return failed }
        // The active loadout is the one the game reads (players/active.txt), so this is its name now.
        playerName = name
        return nil
    }
}

/// Dock menu items call back into the model through this target.
@MainActor final class DockMenuTarget: NSObject {
    private final class Action: NSObject { let run: @MainActor () -> Void; init(_ run: @escaping @MainActor () -> Void) { self.run = run } }
    func item(_ title: String, enabled: Bool, _ run: @escaping @MainActor () -> Void) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: enabled ? #selector(runItem(_:)) : nil, keyEquivalent: "")
        item.target = self
        item.representedObject = Action(run)
        item.isEnabled = enabled
        return item
    }
    @objc func runItem(_ sender: NSMenuItem) { (sender.representedObject as? Action)?.run() }
}

/// The two notifications the launcher posts (Surfaces board). Permission is asked the first time
/// one would be useful, never at launch. A process without an app bundle posts nothing.
@MainActor enum ShellNotifications {
    enum Kind: String { case update, shaders }
    static var available: Bool { Bundle.main.bundleURL.pathExtension == "app" && Bundle.main.bundleIdentifier != nil }
    static func post(id: String, title: String, body: String, kind: Kind) {
        guard available else { return }
        let center = UNUserNotificationCenter.current()
        Task { @MainActor in
            guard (try? await center.requestAuthorization(options: [.alert, .sound])) == true else { return }
            let content = UNMutableNotificationContent()
            content.title = title; content.body = body; content.userInfo = ["kind": kind.rawValue]
            try? await center.add(UNNotificationRequest(identifier: id, content: content, trigger: nil))
        }
    }
}
