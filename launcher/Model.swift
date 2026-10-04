import AppKit
import SwiftUI

@MainActor final class LauncherModel: ObservableObject {
    @Published var page: LauncherPage = .home
    @Published var onboard = true
    @Published var step = 0
    @Published var dataPath = ""
    @Published var keyInput = ""
    @Published var shaderBusy = false
    @Published var shaderProgress = 0.0
    @Published var shaderMessage = "Original rendering, prepared on your Mac."
    @Published var approximate = false
    @Published var settings = GameSettings()
    @Published var library = LauncherLibrary()
    @Published var servers: [GameServer] = []
    @Published var selectedServer: String?
    @Published var refreshing = false
    @Published var queryProgress = ""
    @Published var search = ""
    @Published var filter = "All servers"
    @Published var sort = "Ping"
    @Published var hideEmpty = false
    @Published var hideFull = false
    @Published var directAddress = ""
    @Published var serverPassword = ""
    @Published var gameRunning = false
    @Published var notice = ""
    @Published var crashReport: URL?
    @Published var updateMessage = "Check for the latest release when you're ready."
    @Published var releaseURL: URL?
    @Published var media: [URL] = []
    @Published var mediaTab = "Demos"
    let home: URL
    let preferences: URL
    let snapshot: Bool
    private var engine: Process?
    private var attachedGame: NSRunningApplication?
    private var attachmentTask: Task<Void, Never>?
    private var connectionTask: Task<Void, Never>?
    private var gameStartedAt = Date()
    private var gameBundle: URL { Bundle.main.bundleURL.appendingPathComponent("Contents/Helpers/CoD2 Game.app") }
    private var activeGamePID: pid_t? {
        if let engine, engine.isRunning { return engine.processIdentifier }
        if let attachedGame, !attachedGame.isTerminated { return attachedGame.processIdentifier }
        return nil
    }
    private var browserTask: Task<Void, Never>?
    private var pendingLink: LaunchLink?
    private var fastPlay = false
    private var setupTask: Task<Void, Never>?
    var forwardedArguments: [String] = []
    var exitAfterGame = false

    init(snapshot: Bool = false) {
        self.snapshot = snapshot
        home = URL(fileURLWithPath: snapshot ? "/Preview/CoD2 Silicon" : LauncherHome())
        preferences = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent(".cod2/preferences")
        if snapshot { seedPreview(); return }
        if let resolution = Bundle.main.object(forInfoDictionaryKey: "CoD2DefaultResolution") as? String { settings.resolution = resolution }
        if let mode = Bundle.main.object(forInfoDictionaryKey: "CoD2DefaultFullscreen") as? String { settings.fullscreen = mode }
        LauncherMigrate()
        if let data = try? Data(contentsOf: home.appendingPathComponent("launcher-settings.json")), let value = try? JSONDecoder().decode(GameSettings.self, from: data) { settings = value }
        if let data = try? Data(contentsOf: home.appendingPathComponent("launcher-library.json")), let value = try? JSONDecoder().decode(LauncherLibrary.self, from: data) { library = value; servers = value.cache }
        dataPath = LauncherFindData(nil) ?? ""
        let keyReady = KeyStore.hasValidKey(at: preferences)
        approximate = !LauncherVerifyShaders()
        let setupComplete = FileManager.default.fileExists(atPath: home.appendingPathComponent(".launcher-setup-complete").path)
        onboard = dataPath.isEmpty || !keyReady || (approximate && !setupComplete)
        step = dataPath.isEmpty ? 0 : (keyReady ? 2 : 1)
    }
    var version: String { Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.1.0" }
    var selected: GameServer? { servers.first { $0.address == selectedServer } }
    var visibleServers: [GameServer] {
        let result = servers.filter {
            (search.isEmpty || [QuakeColors.plain($0.name), $0.map, $0.mod, $0.address].joined(separator: " ").localizedCaseInsensitiveContains(search)) &&
            (filter != "Favorites" || library.favorites.contains($0.address)) &&
            (filter != "Recent" || library.recent.contains($0.address)) &&
            (filter != "Stock 1.3" || $0.version.hasPrefix("1.3")) &&
            (filter != "CoD2x 1.4" || $0.version.hasPrefix("1.4")) &&
            (!hideEmpty || $0.playerCount > 0) && (!hideFull || $0.playerCount < $0.maxPlayers)
        }
        return result.sorted {
            switch sort {
            case "Players": if $0.playerCount != $1.playerCount { return $0.playerCount > $1.playerCount }
            case "Name": return QuakeColors.plain($0.name).localizedStandardCompare(QuakeColors.plain($1.name)) == .orderedAscending
            default: if $0.ping != $1.ping { return $0.ping < $1.ping }
            }
            return $0.address < $1.address
        }
    }
    func boot() {
        guard !snapshot else { return }
        _ = attachExistingGame()
        let args = CommandLine.arguments
        fastPlay = args.contains("--play")
        exitAfterGame = args.contains("--exit-after-game")
        if let separator = args.firstIndex(of: "--") { forwardedArguments = Array(args.dropFirst(separator + 1)) }
        if let url = args.first(where: { $0.hasPrefix("cod2x://") }) { openLink(url) }
        if ProcessInfo.processInfo.environment["COD2_SETUP_NONINTERACTIVE"] == "1" {
            setupTask = Task {
                do {
                    guard let data = LauncherFindData(nil) else { throw LauncherError(message: "Game data is missing or incomplete.") }
                    dataPath = data
                    if !KeyStore.hasValidKey(at: preferences) {
                        try KeyStore.save(ProcessInfo.processInfo.environment["COD2_SETUP_CD_KEY"] ?? "", at: preferences)
                    }
                    try rememberData()
                    shaderBusy = true
                    approximate = !(await Task.detached(priority: .utility) { LauncherPrepareShaders(data, nil) }.value)
                    shaderBusy = false
                    try PrivateFile.write(Data("prepared\n".utf8), to: home.appendingPathComponent(".launcher-setup-complete"))
                    onboard = false
                    if fastPlay || pendingLink != nil { play() }
                } catch { notice = error.localizedDescription; if exitAfterGame { fputs("Launcher setup failed.\n", stderr); NSApp.terminate(nil) } }
            }
        } else if onboard && step == 2 { prepareShaders() }
        else if (fastPlay || pendingLink != nil) && !onboard { play() }
    }
    func pickData() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.treatsFilePackagesAsDirectories = true
        panel.message = "Choose your licensed Call of Duty 2 folder containing main/iw_00.iwd through iw_15.iwd."
        guard panel.runModal() == .OK, let selected = panel.url?.path else { return }
        if let valid = LauncherFindData(selected) { dataPath = valid; notice = "" }
        else { notice = "This folder is incomplete. All 16 main/iw_00.iwd–iw_15.iwd archives must be readable." }
    }
    func rememberData() throws {
        guard !dataPath.isEmpty, GameSettings.safeValue(dataPath), GameSettings.safeValue(home.path) else { throw LauncherError(message: "Choose a path without quotes, plus signs, semicolons or control characters.") }
        try PrivateFile.write(Data((dataPath + "\n").utf8), to: home.appendingPathComponent("data-path.txt"))
    }
    func nextSetup() {
        do {
            if step == 0 {
                guard let data = LauncherFindData(dataPath) else { throw LauncherError(message: "Choose a complete licensed game data folder.") }
                dataPath = data; try rememberData(); step = 1
                if KeyStore.hasValidKey(at: preferences) { step = 2; prepareShaders() }
            } else if step == 1 {
                try KeyStore.save(keyInput, at: preferences); keyInput = ""; step = 2; prepareShaders()
            } else { finishSetup() }
            notice = ""
        } catch { notice = error.localizedDescription }
    }
    func prepareShaders(selected: String? = nil) {
        guard !shaderBusy else { return }
        shaderBusy = true; shaderProgress = 0.2; shaderMessage = "Checking the cache and locating your licensed Mac executable…"
        let data = dataPath
        setupTask = Task {
            shaderProgress = 0.5; shaderMessage = "Extracting and verifying 834 shader payloads…"
            let verified = await Task.detached(priority: .utility) { LauncherPrepareShaders(data, selected) }.value
            shaderProgress = 1; shaderBusy = false; approximate = !verified
            shaderMessage = verified ? "Original shaders verified. You're ready to play." : "Original Mac shaders weren't found. Approximate shaders change some lighting and skies."
        }
    }
    func pickShaders() {
        let panel = NSOpenPanel(); panel.canChooseDirectories = true; panel.canChooseFiles = true; panel.treatsFilePackagesAsDirectories = true
        panel.message = "Choose your own Mac Call of Duty 2 folder or multiplayer executable."
        if panel.runModal() == .OK, let url = panel.url { prepareShaders(selected: url.path) }
    }
    func finishSetup() {
        do {
            try PrivateFile.write(Data("prepared\n".utf8), to: home.appendingPathComponent(".launcher-setup-complete"))
            onboard = false; if fastPlay || pendingLink != nil { play() }
        } catch { notice = error.localizedDescription }
    }
    func saveSettings() {
        do {
            let text = try settings.config()
            try PrivateFile.write(Data(text.utf8), to: home.appendingPathComponent("main/launcher.cfg"))
            try PrivateFile.write(JSONEncoder().encode(settings), to: home.appendingPathComponent("launcher-settings.json"))
            notice = "Settings saved. They apply next time you play."
        } catch { notice = error.localizedDescription }
    }
    func saveLibrary() {
        guard !snapshot else { return }
        do { try PrivateFile.write(JSONEncoder().encode(library), to: home.appendingPathComponent("launcher-library.json")) }
        catch { notice = "Could not save your server library." }
    }
    func favorite(_ address: String) {
        if library.favorites.contains(address) { library.favorites.remove(address) } else { library.favorites.insert(address) }
        saveLibrary()
    }
    func refresh() {
        guard !snapshot, !refreshing else { return }
        refreshing = true; queryProgress = "Contacting both master servers…"
        browserTask = Task {
            let discovered = await ServerQueries.masterAddresses()
            let addresses = Array(Set(discovered).union(library.favorites).union(library.recent)).sorted()
            if discovered.isEmpty && !Task.isCancelled { notice = "Master servers did not respond. Cached servers and direct connect remain available." }
            var fresh: [GameServer] = []
            for start in stride(from: 0, to: addresses.count, by: 6) {
                if Task.isCancelled { break }
                let batch = Array(addresses[start..<min(start + 6, addresses.count)])
                await withTaskGroup(of: GameServer?.self) { group in
                    for address in batch {
                        group.addTask { await ServerQueries.server(address) }
                        try? await Task.sleep(for: .milliseconds(40))
                    }
                    for await value in group { if let value { fresh.append(value) } }
                }
                servers = fresh + library.cache.filter { cached in !fresh.contains { $0.address == cached.address } }
                queryProgress = "Queried \(min(start + 6, addresses.count)) of \(addresses.count) · \(fresh.count) responding"
            }
            if fresh.isEmpty && !Task.isCancelled { notice = "No servers responded. Your cached results remain available; try direct connect or refresh later." }
            if !fresh.isEmpty { servers = fresh; library.cache = fresh; library.cachedAt = Date(); saveLibrary() }
            refreshing = false
        }
    }
    func cancelRefresh() { browserTask?.cancel() }
    func connect(_ server: GameServer? = nil) {
        do { pendingLink = try LaunchLink.direct(server?.address ?? directAddress, password: serverPassword); serverPassword = ""; play() }
        catch { notice = error.localizedDescription }
    }
    func openLink(_ url: String) {
        do {
            let link = try LaunchLink(url)
            if let pid = activeGamePID {
                scheduleLink(link, pid: pid)
            } else { pendingLink = link; fastPlay = true; if !onboard { play() } }
        } catch { notice = error.localizedDescription }
    }
    private func scheduleLink(_ link: LaunchLink, pid: pid_t) {
        connectionTask?.cancel()
        connectionTask = Task {
            for _ in 0..<150 {
                guard !Task.isCancelled, activeGamePID == pid else { return }
                if NSRunningApplication(processIdentifier: pid)?.isFinishedLaunching == true {
                    do {
                        let event = NSAppleEventDescriptor(eventClass: AEEventClass(kInternetEventClass), eventID: AEEventID(kAEGetURL), targetDescriptor: NSAppleEventDescriptor(processIdentifier: pid), returnID: AEReturnID(kAutoGenerateReturnID), transactionID: AETransactionID(kAnyTransactionID))
                        event.setParam(NSAppleEventDescriptor(string: link.url), forKeyword: AEKeyword(keyDirectObject))
                        _ = try event.sendEvent(options: .noReply, timeout: 2)
                        library.joined(link.address); saveLibrary()
                        NSRunningApplication(processIdentifier: pid)?.activate(options: .activateAllWindows)
                    } catch { notice = "Could not deliver the server link to the game. Try again after the menu appears." }
                    return
                }
                try? await Task.sleep(for: .milliseconds(200))
            }
            notice = "The game hasn't finished starting. Open the server link again after the menu appears."
        }
    }
    private func attachExistingGame() -> Bool {
        guard !snapshot, !gameRunning, let identifier = Bundle.main.bundleIdentifier else { return false }
        let expected = gameBundle.appendingPathComponent("Contents/MacOS/cod2_macos").resolvingSymlinksInPath()
        guard let app = NSRunningApplication.runningApplications(withBundleIdentifier: identifier + ".game").first(where: {
            !$0.isTerminated && $0.executableURL?.resolvingSymlinksInPath() == expected
        }) else { return false }
        attachedGame = app; gameRunning = true; gameStartedAt = Date()
        NSApp.setActivationPolicy(.accessory)
        NSApp.windows.forEach { $0.orderOut(nil) }
        print("CoD2 Silicon: reconnected to existing game (pid \(app.processIdentifier)).")
        fflush(stdout)
        attachmentTask = Task { [weak self] in
            while !app.isTerminated { try? await Task.sleep(for: .milliseconds(250)); if Task.isCancelled { return } }
            self?.attachedGame = nil
            self?.gameEnded(code: nil)
        }
        return true
    }
    func play(demo: URL? = nil) {
        guard !snapshot, !gameRunning else { return }
        if attachExistingGame() {
            if let link = pendingLink { pendingLink = nil; openLink(link.url) }
            return
        }
        if onboard { return }
        do {
            try rememberData()
            let bundle = gameBundle
            let executable = bundle.appendingPathComponent("Contents/MacOS/cod2_macos")
            guard FileManager.default.isExecutableFile(atPath: executable.path) else { throw LauncherError(message: "The game helper is missing. Rebuild or reinstall this app.") }
            let plan = try GameLaunchPlan(settings: settings, dataPath: dataPath, homePath: home.path, link: pendingLink,
                demo: demo?.deletingPathExtension().lastPathComponent, forwarded: forwardedArguments)
            let process = Process(); process.executableURL = executable; process.arguments = plan.arguments
            process.currentDirectoryURL = home
            var environment = ProcessInfo.processInfo.environment
            environment["HOME"] = NSHomeDirectory()
            environment.removeValue(forKey: "COD2_SETUP_CD_KEY")
            environment["SDL_VIDEO_MAC_FULLSCREEN_SPACES"] = settings.fullscreen == "spaces" ? "1" : "0"
            if !approximate { environment["COD2_MAC_SHADER_CACHE"] = home.appendingPathComponent("shaders").path }
            else { environment.removeValue(forKey: "COD2_MAC_SHADER_CACHE") }
            process.environment = environment
            process.standardInput = FileHandle.standardInput
            process.standardOutput = FileHandle.standardOutput; process.standardError = FileHandle.standardError
            process.terminationHandler = { [weak self] process in
                let code = process.terminationStatus
                Task { @MainActor in self?.gameEnded(code: code) }
            }
            try PrivateFile.write(Data(try settings.config().utf8), to: home.appendingPathComponent("main/launcher.cfg"))
            try process.run(); engine = process; gameRunning = true; gameStartedAt = Date(); crashReport = nil; notice = ""
            pendingLink = nil
            if let link = plan.link { scheduleLink(link, pid: process.processIdentifier) }
            cancelRefresh()
            NSApp.windows.forEach { $0.orderOut(nil) }
            NSApp.setActivationPolicy(.accessory)
            print("CoD2 Silicon: game started (pid \(process.processIdentifier)).")
        } catch { notice = error.localizedDescription }
    }
    private func gameEnded(code: Int32?) {
        connectionTask?.cancel()
        engine = nil; gameRunning = false
        NSApp.setActivationPolicy(.regular)
        NSApp.windows.first?.makeKeyAndOrderFront(nil); NSApp.activate(ignoringOtherApps: true)
        if let code, code != 0 {
            notice = "The game exited unexpectedly (\(code)). Your launcher is still open."
        }
        if code == nil || code != 0 {
            crashReport = (try? FileManager.default.contentsOfDirectory(at: home, includingPropertiesForKeys: [.contentModificationDateKey]))?
                .filter { $0.lastPathComponent.hasPrefix("cod2_crash_") && $0.pathExtension == "txt" && ((try? $0.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast) >= gameStartedAt }
                .sorted { ((try? $0.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast) > ((try? $1.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast) }.first
        }
        if code == nil && crashReport != nil { notice = "The game closed and produced a crash report. Your launcher is still open." }
        print("CoD2 Silicon: returned to launcher (game exit \(code.map(String.init) ?? "unavailable")).")
        fflush(stdout)
        if pendingLink != nil { play() }
        else if exitAfterGame { NSApp.terminate(nil) }
    }
    func loadMedia() {
        guard !snapshot else { return }
        let folder = home.appendingPathComponent(mediaTab == "Demos" ? "main/demos" : "main/screenshots")
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let extensions = mediaTab == "Demos" ? ["dm_1"] : ["jpg", "jpeg", "png", "tga"]
        media = ((try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.contentModificationDateKey])) ?? []).filter { extensions.contains($0.pathExtension.lowercased()) }.sorted { $0.lastPathComponent < $1.lastPathComponent }
    }
    func openMediaFolder() { NSWorkspace.shared.open(home.appendingPathComponent(mediaTab == "Demos" ? "main/demos" : "main/screenshots")) }
    func checkUpdates() {
        guard !snapshot else { return }
        updateMessage = "Checking GitHub Releases…"
        Task {
            do {
                var request = URLRequest(url: URL(string: "https://api.github.com/repos/STiXzoOR/cod2-silicon/releases/latest")!)
                request.timeoutInterval = 15; request.setValue("CoD2-Silicon-Launcher", forHTTPHeaderField: "User-Agent")
                let (data, response) = try await URLSession.shared.data(for: request)
                guard let response = response as? HTTPURLResponse, response.statusCode == 200, data.count < 1_000_000 else { throw LauncherError(message: "Release service is unavailable. Try again later.") }
                struct Release: Decodable { var tag_name: String; var html_url: String }
                let release = try JSONDecoder().decode(Release.self, from: data)
                let latest = release.tag_name.trimmingCharacters(in: CharacterSet(charactersIn: "v"))
                if latest.compare(version, options: .numeric) == .orderedDescending {
                    updateMessage = "Version \(latest) is available. Download it when you're ready."
                    releaseURL = URL(string: "https://github.com/STiXzoOR/cod2-silicon/releases")
                } else { updateMessage = "You're up to date · \(version)"; releaseURL = nil }
            } catch { updateMessage = error.localizedDescription }
        }
    }
    private func seedPreview() {
        onboard = false; dataPath = "/Users/Player/Games/CoD2"
        let names = ["^5Northern Lights ^7| Public", "^2Sunday Rifles ^7• EU", "^3The Toujane Club", "^1Classic ^7Headquarters", "^5Silicon Sessions"]
        servers = names.enumerated().map { index, name in
            GameServer(address: "192.0.2.\(index + 10):28960", fields: ["sv_hostname": name, "mapname": ["mp_toujane", "mp_carentan", "mp_burgundy", "mp_dawnville", "mp_matmata"][index], "g_gametype": index == 2 ? "sd" : "tdm", "sv_maxclients": "24", "clients": String(18 - index * 3), "shortversion": index % 2 == 0 ? "1.4.6.8" : "1.3", "fs_game": index == 2 ? "PAM" : "", "pswrd": index == 4 ? "1" : "0", "sv_maxfps": "250"], ping: 18 + index * 12, players: (0..<6).map { ServerPlayer(id: $0, score: 42 - $0 * 5, ping: 20 + $0 * 7, name: ["^5Aurora", "North", "^2Moss", "Echo", "Scout", "River"][$0]) })
        }
        selectedServer = servers.first?.address
        library.favorites = [servers[0].address, servers[2].address]; library.recent = [servers[0].address]
        library.cachedAt = Date(); media = ["toujane-evening.dm_1", "rifle-practice.dm_1", "carentan-round.dm_1"].map { home.appendingPathComponent("main/demos/" + $0) }
    }
}
