import AppKit
import SwiftUI

enum LauncherPage: String, CaseIterable, Identifiable, Hashable {
    case deploy, servers, library, configs, settings, about
    var id: String { rawValue }
    var title: String {
        switch self { case .deploy: "Deploy"; case .servers: "Servers"; case .library: "Library"; case .configs: "Configs"; case .settings: "Settings"; case .about: "About" }
    }
    var symbol: String {
        switch self {
        case .deploy: "play.fill"; case .servers: "dot.radiowaves.left.and.right"; case .library: "film"
        case .configs: "square.stack"; case .settings: "slider.horizontal.3"; case .about: "info.circle"
        }
    }
    static let navigation: [LauncherPage] = [.deploy, .servers, .library, .configs, .settings]
}

enum ServerFilter: String, CaseIterable, Identifiable { case all = "All", stock = "Stock 1.3", codx = "CoD2x 1.4"; var id: String { rawValue } }
enum ServerSort: String { case ping, players, name }
enum LibraryTab: String, CaseIterable, Identifiable { case demos = "Demos", screenshots = "Screenshots"; var id: String { rawValue } }

struct MediaEntry: Identifiable {
    var url: URL
    var facts: MediaFacts
    var id: String { url.path }
}

@MainActor final class LauncherModel: ObservableObject {
    @Published var page: LauncherPage = .deploy
    @Published var onboard = true
    @Published var step = 0
    @Published var dataPath = ""
    @Published var keyInput = ""
    @Published var shaderBusy = false
    @Published var shaderCount = 0
    @Published var shaderMessage = "Lighting and sky exactly as the game shipped, taken from your own Mac copy of Call of Duty 2."
    @Published var approximate = false
    @Published var settings = GameSettings() {
        didSet {
            if settings != savedSettings { settingsSaved = false }
            if settings.matchSummariesEnabled != oldValue.matchSummariesEnabled {
                studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
            }
        }
    }
    /// True right after Save, until the next change.
    @Published var settingsSaved = false
    @Published var library = LauncherLibrary()
    @Published var servers: [GameServer] = []
    @Published var selectedServer: String? {
        didSet { do { try serverConnection.select(address: selectedServer) } catch { serverConnection.clear(); notice = error.localizedDescription } }
    }
    @Published var refreshing = false
    @Published var queryProgress = ""
    @Published var search = ""
    @Published var filter: ServerFilter = .all
    @Published var sort: ServerSort = .ping
    @Published var hideEmpty = false
    @Published var hideFull = false
    @Published var directAddress = "" {
        didSet {
            guard directAddress != oldValue else { return }
            do { try serverConnection.select(address: directAddress.isEmpty ? nil : directAddress) }
            catch { try? serverConnection.select(address: nil) }
        }
    }
    @Published var serverPassword = ""
    @Published var gameRunning = false
    @Published var notice = ""
    @Published var crashReport: URL?
    @Published var updateMessage = "Check for the latest release when you're ready."
    @Published var checkingUpdates = false
    @Published var releaseURL: URL?
    @Published var media: [MediaEntry] = []
    @Published var libraryTab: LibraryTab = .demos
    @Published var playerName = PlayerProfile.unnamed
    @Published var keyVerified = false
    let artwork: MapArtworkStore
    let home: URL
    let preferences: URL
    let snapshot: Bool
    let configStudio: ConfigStudioSession?
    let serverConnection = ConfigServerConnection()
    private lazy var readyRules = try? ConfigRules.load()
    private var readyChecker: ConfigReadyCheck? {
        guard let catalog = studio.catalog, let rules = readyRules else { return nil }
        return ConfigReadyCheck(catalog: catalog, rules: rules)
    }
    /// The Configs page's view store over the same session.
    let studio: ConfigStudioModel
    /// Discover and import. The player directory has no signed publication yet, so it reports itself unavailable.
    let discover: ConfigDiscoverModel
    private var savedSettings = GameSettings()
    private var engine: Process?
    private var attachedGames: [pid_t: NSRunningApplication] = [:]
    private var attachmentTask: Task<Void, Never>?
    private var connectionTask: Task<Void, Never>?
    private var handOffTask: Task<Void, Never>?
    private var gameStartedAt = Date()
    private var lastExitCode: Int32?
    private var gameBundle: URL { Bundle.main.bundleURL.appendingPathComponent("Contents/Helpers/CoD2 Game.app") }
    private var activeGamePID: pid_t? {
        if let engine, engine.isRunning { return engine.processIdentifier }
        if let app = attachedGames.values.filter({ !$0.isTerminated }).sorted(by: { $0.processIdentifier < $1.processIdentifier }).first { return app.processIdentifier }
        return nil
    }
    private var browserTask: Task<Void, Never>?
    private var pendingLink: LaunchLink?
    private var fastPlay = false
    private var booted = false
    private var setupTask: Task<Void, Never>?
    var forwardedArguments: [String] = []
    var exitAfterGame = false

    init(snapshot: Bool = false) {
        self.snapshot = snapshot
        home = URL(fileURLWithPath: snapshot ? "/Preview/CoD2 Silicon" : LauncherHome())
        artwork = MapArtworkStore(home: home)
        preferences = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent(".cod2/preferences")
        let expected = Bundle.main.bundleURL.appendingPathComponent("Contents/Helpers/CoD2 Game.app/Contents/MacOS/cod2_macos").resolvingSymlinksInPath()
        let identifier = (Bundle.main.bundleIdentifier ?? "") + ".game"
        var sessionError: String?
        if snapshot { configStudio = nil }
        else {
            do {
                configStudio = try ConfigStudioSession(home: home, dialect: .cod2x, discoverGames: {
                    Set(NSRunningApplication.runningApplications(withBundleIdentifier: identifier).filter {
                        !$0.isTerminated && $0.executableURL?.resolvingSymlinksInPath() == expected
                    }.map(\.processIdentifier))
                })
            } catch { configStudio = nil; sessionError = error.localizedDescription }
        }
        studio = ConfigStudioModel(session: configStudio, dialect: .cod2x)
        discover = ConfigDiscoverModel(studio: studio, directory: ConfigDirectory(cacheURL: home.appendingPathComponent("directory/index.json")))
        discover.notify = { [weak self] in self?.notice = $0 }
        if let sessionError { notice = sessionError }
        if snapshot { seedPreview(); return }
        if let resolution = Bundle.main.object(forInfoDictionaryKey: "CoD2DefaultResolution") as? String { settings.resolution = resolution }
        if let mode = Bundle.main.object(forInfoDictionaryKey: "CoD2DefaultFullscreen") as? String { settings.fullscreen = mode }
        LauncherMigrate()
        if let data = try? Data(contentsOf: home.appendingPathComponent("launcher-settings.json")), let value = try? JSONDecoder().decode(GameSettings.self, from: data) { settings = value }
        savedSettings = settings
        studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
        if let data = try? Data(contentsOf: home.appendingPathComponent("launcher-library.json")), let value = try? JSONDecoder().decode(LauncherLibrary.self, from: data) { library = value; servers = value.cache }
        dataPath = LauncherFindData(nil) ?? ""
        keyVerified = KeyStore.hasValidKey(at: preferences)
        approximate = !LauncherVerifyShaders()
        readPlayerName()
        let setupComplete = FileManager.default.fileExists(atPath: home.appendingPathComponent(".launcher-setup-complete").path)
        onboard = dataPath.isEmpty || !keyVerified || (approximate && !setupComplete)
        step = dataPath.isEmpty ? 0 : (keyVerified ? 2 : 1)
        if !onboard { step = 0 }
    }
    var version: String { Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.1.0" }
    var selected: GameServer? { servers.first { $0.address == selectedServer } ?? visibleServers.first }
    var lastServer: GameServer? {
        guard let address = library.recent.first else { return nil }
        return servers.first { $0.address == address } ?? library.cache.first { $0.address == address }
    }
    var recentServers: [GameServer] {
        library.recent.prefix(12).compactMap { address in servers.first { $0.address == address } ?? library.cache.first { $0.address == address } }
    }
    var favoriteServers: [GameServer] {
        library.favorites.sorted().compactMap { address in servers.first { $0.address == address } ?? library.cache.first { $0.address == address } }
    }
    var dispatches: [Dispatch] { library.dispatches.isEmpty ? Dispatch.bundled : Array(library.dispatches.prefix(2)) }
    var dataPathDisplay: String { (dataPath as NSString).abbreviatingWithTildeInPath }
    var visibleServers: [GameServer] {
        let result = servers.filter {
            (search.isEmpty || [QuakeColors.plain($0.name), $0.map, MapCatalog.displayName($0.map), $0.mod, $0.address].joined(separator: " ").localizedCaseInsensitiveContains(search)) &&
            (filter != .stock || !ServerFacts($0).isCoD2x) && (filter != .codx || ServerFacts($0).isCoD2x) &&
            (!hideEmpty || $0.playerCount > 0) && (!hideFull || $0.maxPlayers == 0 || $0.playerCount < $0.maxPlayers)
        }
        return result.sorted {
            switch sort {
            case .players: if $0.playerCount != $1.playerCount { return $0.playerCount > $1.playerCount }
            case .name: return QuakeColors.plain($0.name).localizedStandardCompare(QuakeColors.plain($1.name)) == .orderedAscending
            case .ping: if $0.ping != $1.ping { return $0.ping < $1.ping }
            }
            return $0.address < $1.address
        }
    }
    func boot() {
        guard !snapshot else { return }
        booted = true
        _ = attachExistingGame()
        startGameMonitor()
        let args = CommandLine.arguments
        fastPlay = args.contains("--play")
        exitAfterGame = args.contains("--exit-after-game")
        if let separator = args.firstIndex(of: "--") { forwardedArguments = Array(args.dropFirst(separator + 1)) }
        if !gameRunning {
            do { try configStudio?.initialize(settings: settings, forwarded: forwardedArguments) }
            catch { notice = error.localizedDescription }
        }
        if let url = args.first(where: { $0.hasPrefix("cod2x://") }) { openLink(url) }
        studio.loadCatalog()
        refreshMatchSummaries()
        if ProcessInfo.processInfo.environment["COD2_SETUP_NONINTERACTIVE"] == "1" {
            setupTask = Task {
                do {
                    guard let data = LauncherFindData(nil) else { throw LauncherError(message: "Game data is missing or incomplete.") }
                    dataPath = data
                    if !KeyStore.hasValidKey(at: preferences) {
                        try KeyStore.save(ProcessInfo.processInfo.environment["COD2_SETUP_CD_KEY"] ?? "", at: preferences)
                    }
                    keyVerified = true
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
        loadArtwork()
    }
    func loadArtwork() {
        guard !snapshot, !dataPath.isEmpty else { return }
        var maps = recentServers.map(\.map) + visibleServers.prefix(48).map(\.map)
        if let selected { maps.append(selected.map) }
        let path = dataPath
        Task { await artwork.load(dataPath: path, maps: maps) }
    }
    /// The in-game name, read the way the engine loads it: the active profile's config_mp.cfg, then main's.
    private func readPlayerName() {
        let main = home.appendingPathComponent("main")
        var configs: [URL] = []
        if let active = try? String(contentsOf: main.appendingPathComponent("players/active.txt"), encoding: .isoLatin1),
           let profile = PlayerProfile.activeProfile(active) {
            configs.append(main.appendingPathComponent("players").appendingPathComponent(profile).appendingPathComponent("config_mp.cfg"))
        }
        configs.append(main.appendingPathComponent("config_mp.cfg"))
        for config in configs {
            guard let values = try? config.resourceValues(forKeys: [.fileSizeKey, .isRegularFileKey]), values.isRegularFile == true,
                  (values.fileSize ?? 0) <= 262_144, let text = try? String(contentsOf: config, encoding: .isoLatin1) else { continue }
            if let name = PlayerProfile.name(config: text) { playerName = name; return }
        }
        playerName = PlayerProfile.unnamed
    }

    // MARK: Setup
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
    var keyReady: Bool { KeyStore.valid(keyInput) }
    func nextSetup() {
        do {
            if step == 0 {
                guard let data = LauncherFindData(dataPath) else { throw LauncherError(message: "Choose a complete licensed game data folder.") }
                dataPath = data; try rememberData()
                withAnimation(.launcherSpring) { step = 1 }
                if KeyStore.hasValidKey(at: preferences) { keyVerified = true; withAnimation(.launcherSpring) { step = 2 }; prepareShaders() }
            } else if step == 1 {
                try KeyStore.save(keyInput, at: preferences); keyInput = ""; keyVerified = true
                withAnimation(.launcherSpring) { step = 2 }; prepareShaders()
            } else { finishSetup() }
            notice = ""
        } catch { notice = error.localizedDescription }
    }
    func backSetup() { if step > 0 && !shaderBusy { withAnimation(.launcherSpring) { step -= 1 } } }
    func prepareShaders(selected: String? = nil) {
        guard !shaderBusy, !snapshot else { return }
        shaderBusy = true; shaderCount = 0; shaderMessage = "Reading your Mac copy of Call of Duty 2…"
        let data = dataPath, parent = home
        let progress = Task { [weak self] in
            while !Task.isCancelled {
                let count = await Task.detached(priority: .utility) { Self.stagedShaderCount(in: parent) }.value
                if let self, self.shaderBusy, let count {
                    self.shaderCount = min(834, max(self.shaderCount, count))
                    self.shaderMessage = "Extracting from Call of Duty 2 Multiplayer…"
                }
                try? await Task.sleep(for: .milliseconds(120))
            }
        }
        setupTask = Task {
            let verified = await Task.detached(priority: .utility) { LauncherPrepareShaders(data, selected) }.value
            progress.cancel()
            withAnimation(.launcherSpring) { shaderCount = verified ? 834 : 0 }
            shaderBusy = false; approximate = !verified
            shaderMessage = verified ? "All shaders verified" : "Original Mac shaders weren't found. Approximate shaders change some lighting and skies."
        }
    }
    /// Payloads written so far into the extractor's private staging folder in the app home.
    nonisolated static func stagedShaderCount(in parent: URL) -> Int? {
        guard let entries = try? FileManager.default.contentsOfDirectory(at: parent, includingPropertiesForKeys: nil) else { return nil }
        let staging = entries.filter { $0.lastPathComponent.hasPrefix(".shaders-") && !$0.lastPathComponent.hasSuffix("-old") }
        guard let folder = staging.first else { return nil }
        let names = (try? FileManager.default.contentsOfDirectory(atPath: folder.path)) ?? []
        return names.filter { !$0.hasPrefix(".") && $0 != "manifest.json" }.count
    }
    func pickShaders() {
        let panel = NSOpenPanel(); panel.canChooseDirectories = true; panel.canChooseFiles = true; panel.treatsFilePackagesAsDirectories = true
        panel.message = "Choose your own Mac Call of Duty 2 folder or multiplayer executable."
        if panel.runModal() == .OK, let url = panel.url { prepareShaders(selected: url.path) }
    }
    func finishSetup() {
        do {
            try PrivateFile.write(Data("prepared\n".utf8), to: home.appendingPathComponent(".launcher-setup-complete"))
            withAnimation(.launcherSpring) { onboard = false; page = .deploy }
            loadArtwork()
            if fastPlay || pendingLink != nil { play() }
        } catch { notice = error.localizedDescription }
    }
    func restartSetup() { keyInput = ""; step = 0; withAnimation(.launcherSpring) { onboard = true } }

    // MARK: Settings and library
    func saveSettings() {
        do {
            let text = try settings.launchConfig(migrated: configStudio?.migrationCompleted == true)
            guard !snapshot else { settingsSaved = true; return }
            try PrivateFile.write(Data(text.utf8), to: home.appendingPathComponent("main/launcher.cfg"))
            try PrivateFile.write(JSONEncoder().encode(settings), to: home.appendingPathComponent("launcher-settings.json"))
            savedSettings = settings; settingsSaved = true
            studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
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
    func showServer(_ address: String) {
        withAnimation(.launcherSpring) { page = .servers; selectedServer = address }
    }

    // MARK: Servers
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
                queryProgress = "Queried \(min(start + 6, addresses.count)) of \(addresses.count)"
            }
            if fresh.isEmpty && !Task.isCancelled { notice = "No servers responded. Your cached results remain available; try direct connect or refresh later." }
            if !fresh.isEmpty { servers = fresh; library.cache = fresh; library.cachedAt = Date(); saveLibrary() }
            refreshing = false
            loadArtwork()
        }
    }
    func cancelRefresh() { browserTask?.cancel() }
    /// The session the ready check reads: Configs' own (the review harness attaches one to a preview).
    private var readySession: ConfigStudioSession? { studio.session ?? configStudio }
    /// A connection alternative does not change the active loadout in Studio.
    func useLoadoutForServer(_ id: String, address: String) throws {
        guard let configStudio = readySession, !configStudio.isReadOnly else { throw ConfigProfileError.gameRunning }
        _ = try configStudio.readProfile(id)
        try serverConnection.select(address: address)
        try serverConnection.useProfile(id)
        objectWillChange.send()
    }
    func readyCheck(for address: String) throws -> ConfigReadyReport? {
        guard let readyChecker, let configStudio = readySession else { return nil }
        let normalized = try LaunchLink.canonicalAddress(address)
        let server = servers.first { (try? LaunchLink.canonicalAddress($0.address)) == normalized }
        let chosenID = serverConnection.address == normalized ? serverConnection.profileID : nil
        guard let profile = try chosenID.map(configStudio.readProfile) ?? configStudio.readActive() else { return nil }
        return readyChecker.evaluate(server: server, profile: profile, mode: .cod2x,
            advancedChain: try configStudio.readAdvancedChain(profileID: profile.profile.id))
    }
    func fixForServerOnly(_ address: String) throws {
        guard let configStudio = readySession, !configStudio.isReadOnly else { throw ConfigProfileError.gameRunning }
        try serverConnection.select(address: address)
        guard let report = try readyCheck(for: address) else { throw ConfigReadyCheckError.noVerifiedFix }
        try serverConnection.fix(report: report)
        objectWillChange.send()
    }
    /// Query one endpoint, including direct-connect addresses absent from the
    /// master list. Selection and staged choices remain the caller's decision.
    func refreshReadyCheck(for address: String) async throws -> ConfigReadyReport? {
        let normalized = try LaunchLink.canonicalAddress(address)
        servers.removeAll { (try? LaunchLink.canonicalAddress($0.address)) == normalized }
        if let server = await ServerQueries.server(normalized) {
            servers.removeAll { (try? LaunchLink.canonicalAddress($0.address)) == normalized }
            servers.append(server)
        }
        return try readyCheck(for: normalized)
    }
    func connect(_ server: GameServer? = nil) {
        if server == nil && directAddress.trimmingCharacters(in: .whitespaces).isEmpty {
            notice = "Enter a server address, such as 203.0.113.24:28960."; return
        }
        do {
            let link = try LaunchLink.direct(server?.address ?? directAddress, password: serverPassword)
            try serverConnection.select(address: link.address)
            serverPassword = ""
            if activeGamePID != nil { openLink(link.url) }
            else { pendingLink = link; play() }
        }
        catch { notice = error.localizedDescription }
    }
    /// Home's Deploy: rejoin the last server, or open the game's menu when there is none.
    func deploy() {
        guard !onboard else { return }
        if let address = library.recent.first, !snapshot {
            do { try serverConnection.select(address: address); pendingLink = try LaunchLink.direct(address); play() } catch { notice = error.localizedDescription }
        } else { play() }
    }
    func openLink(_ url: String) {
        do {
            let link = try LaunchLink(url)
            try serverConnection.select(address: link.address)
            if let pid = activeGamePID {
                // Startup overrides cannot be applied to an already running
                // game through the read-only launch API.
                serverConnection.clear()
                scheduleLink(link, pid: pid)
            } else {
                // A cold link can arrive before launch finishes; boot() starts the game then, so the
                // Dock hand-off always runs after AppKit's own launch-time activation.
                pendingLink = link; fastPlay = true; if booted && !onboard { play() }
            }
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

    // MARK: Game lifecycle
    private func attachExistingGame() -> Bool {
        guard !snapshot, !gameRunning, let identifier = Bundle.main.bundleIdentifier else { return false }
        let expected = gameBundle.appendingPathComponent("Contents/MacOS/cod2_macos").resolvingSymlinksInPath()
        let apps = NSRunningApplication.runningApplications(withBundleIdentifier: identifier + ".game").filter {
            !$0.isTerminated && $0.executableURL?.resolvingSymlinksInPath() == expected
        }
        guard let app = apps.first else { return false }
        attachedGames = Dictionary(uniqueKeysWithValues: apps.map { ($0.processIdentifier, $0) })
        gameRunning = true; gameStartedAt = Date()
        do { try configStudio?.reconcileGames(Set(attachedGames.keys)) } catch { notice = error.localizedDescription }
        NSApp.setActivationPolicy(.accessory)
        NSApp.windows.forEach { $0.orderOut(nil) }
        print("CoD2 Silicon: reconnected to existing game (pid \(app.processIdentifier)).")
        fflush(stdout)
        startGameMonitor()
        return true
    }
    private func startGameMonitor() {
        guard attachmentTask == nil, !snapshot else { return }
        attachmentTask = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(250))
                guard let self, !Task.isCancelled else { return }
                self.synchronizeGames()
            }
        }
    }
    private func synchronizeGames() {
        if let engine, !engine.isRunning { lastExitCode = engine.terminationStatus; self.engine = nil }
        let expected = gameBundle.appendingPathComponent("Contents/MacOS/cod2_macos").resolvingSymlinksInPath()
        let identifier = (Bundle.main.bundleIdentifier ?? "") + ".game"
        let apps = NSRunningApplication.runningApplications(withBundleIdentifier: identifier).filter {
            !$0.isTerminated && $0.executableURL?.resolvingSymlinksInPath() == expected
        }
        attachedGames = Dictionary(uniqueKeysWithValues: apps.map { ($0.processIdentifier, $0) })
        var pids = Set(attachedGames.keys)
        if let engine, engine.isRunning { pids.insert(engine.processIdentifier) }
        let wasRunning = gameRunning
        if !pids.isEmpty && !wasRunning { gameStartedAt = Date() }
        gameRunning = !pids.isEmpty
        do { try configStudio?.reconcileGames(pids) } catch { notice = error.localizedDescription }
        if wasRunning && !gameRunning { gameEnded(code: lastExitCode); lastExitCode = nil }
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
            var connectionLaunch: ConfigConnectionLaunch?
            var connectionProfileID: String?
            var connectionExpectedVersion: ConfigProfileVersion?
            var connectionChainBytes: Data?
            if demo == nil, let link = pendingLink, serverConnection.address == (try LaunchLink.canonicalAddress(link.address)) {
                connectionProfileID = serverConnection.profileID
                if let configStudio {
                    try configStudio.refresh()
                    if let profile = try connectionProfileID.map(configStudio.readProfile) ?? configStudio.readActive(),
                       let report = try readyCheck(for: link.address) {
                        connectionLaunch = try serverConnection.launch(for: link, profile: profile, report: report)
                        if connectionLaunch != nil {
                            connectionProfileID = profile.profile.id; connectionExpectedVersion = profile.version
                            connectionChainBytes = report.advancedChainBytes
                        }
                    } else if !serverConnection.overrides.isEmpty { throw ConfigReadyCheckError.staleReview }
                }
            }
            let configLaunch = try configStudio?.prepareLaunch(settings: settings, forwarded: forwardedArguments,
                connectionProfileID: connectionProfileID, connection: connectionLaunch,
                connectionExpectedVersion: connectionExpectedVersion, connectionChainBytes: connectionChainBytes)
            let plan = try GameLaunchPlan(settings: settings, dataPath: dataPath, homePath: home.path, link: pendingLink,
                demo: demo?.deletingPathExtension().lastPathComponent, forwarded: forwardedArguments, configStudio: configLaunch, connection: connectionLaunch)
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
                let pid = process.processIdentifier
                Task { @MainActor in
                    guard let self, self.engine?.processIdentifier == pid else { return }
                    self.lastExitCode = code; self.engine = nil; self.synchronizeGames()
                }
            }
            try PrivateFile.write(Data(try settings.launchConfig(migrated: configLaunch != nil).utf8), to: home.appendingPathComponent("main/launcher.cfg"))
            try process.run(); engine = process; gameRunning = true; gameStartedAt = Date(); crashReport = nil; notice = ""
            configStudio?.gameLaunched(pid: process.processIdentifier)
            if let error = configStudio?.migrationError ?? configStudio?.error { notice = error }
            startGameMonitor()
            // --play and cold links launch once: a later setup completion must not start a second game.
            pendingLink = nil; fastPlay = false; serverConnection.clear()
            if let link = plan.link { scheduleLink(link, pid: process.processIdentifier) }
            cancelRefresh()
            handOff(to: process.processIdentifier)
            print("CoD2 Silicon: game started (pid \(process.processIdentifier)).")
            fflush(stdout)
        } catch {
            notice = error.localizedDescription
            do { try configStudio?.cancelLaunch() } catch { notice += " " + error.localizedDescription }
        }
    }
    /// Gives the game keyboard and mouse focus. Since macOS 14 an app can only take focus when
    /// the active app yields it, so the launcher stays active until the game has launched,
    /// yields and activates it, and only then hides its window and leaves the Dock.
    private func handOff(to pid: pid_t) {
        handOffTask?.cancel()
        handOffTask = Task { @MainActor [weak self] in
            for _ in 0..<150 {
                guard let self, self.gameRunning, !Task.isCancelled else { return }
                if let game = NSRunningApplication(processIdentifier: pid), game.isFinishedLaunching {
                    if #available(macOS 14.0, *) {
                        NSApp.yieldActivation(to: game)
                        game.activate(from: NSRunningApplication.current, options: [.activateAllWindows])
                    } else {
                        game.activate(options: [.activateAllWindows, .activateIgnoringOtherApps])
                    }
                    break
                }
                try? await Task.sleep(for: .milliseconds(100))
            }
            guard let self, self.gameRunning else { return }
            NSApp.windows.forEach { $0.orderOut(nil) }
            NSApp.setActivationPolicy(.accessory)
        }
    }
    /// Takes the Dock back after the game. The system can refuse the switch (seen right after a
    /// LaunchServices launch); AppKit then already reports .regular, so step through accessory and retry.
    private func restoreRegularPolicy() {
        guard !NSApp.setActivationPolicy(.regular) else { return }
        NSApp.setActivationPolicy(.accessory)
        guard !NSApp.setActivationPolicy(.regular) else { return }
        Task {
            for _ in 0..<10 {
                try? await Task.sleep(for: .milliseconds(250))
                guard !gameRunning else { return }
                NSApp.setActivationPolicy(.accessory)
                if NSApp.setActivationPolicy(.regular) { NSApp.activate(ignoringOtherApps: true); return }
            }
        }
    }
    private func gameEnded(code: Int32?) {
        connectionTask?.cancel(); handOffTask?.cancel()
        engine = nil; gameRunning = false
        restoreRegularPolicy()
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
        readPlayerName()
        refreshMatchSummaries()
        if pendingLink != nil { play() }
        else if exitAfterGame { NSApp.terminate(nil) }
    }

    /// Check-up reads the game's local match summaries from this home at startup and after each
    /// game; the 250 ms game monitor never reads them. The switch shown applies on the next launch.
    private func refreshMatchSummaries() {
        studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
        studio.checkup.refreshMatches(home: home)
    }

    // MARK: Library, folders and updates
    func loadMedia() {
        guard !snapshot else { return }
        let folder = home.appendingPathComponent(libraryTab == .demos ? "main/demos" : "main/screenshots")
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let extensions = libraryTab == .demos ? ["dm_1"] : ["jpg", "jpeg", "png", "tga"]
        let keys: [URLResourceKey] = [.contentModificationDateKey, .fileSizeKey]
        media = ((try? FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: keys)) ?? [])
            .filter { extensions.contains($0.pathExtension.lowercased()) }
            .map { url in
                let values = try? url.resourceValues(forKeys: Set(keys))
                return MediaEntry(url: url, facts: MediaFacts(name: url.lastPathComponent, date: values?.contentModificationDate, bytes: values?.fileSize))
            }
            .sorted { ($0.url.lastPathComponent) > ($1.url.lastPathComponent) }
    }
    func openMediaFolder() { NSWorkspace.shared.open(home.appendingPathComponent(libraryTab == .demos ? "main/demos" : "main/screenshots")) }
    func openHomeFolder() { NSWorkspace.shared.open(home) }
    /// Explicit HTTPS request to GitHub Releases. Notify only: nothing is downloaded or installed.
    func checkUpdates() {
        guard !snapshot, !checkingUpdates else { return }
        checkingUpdates = true; updateMessage = "Checking GitHub Releases…"
        Task {
            defer { checkingUpdates = false }
            do {
                var request = URLRequest(url: URL(string: "https://api.github.com/repos/STiXzoOR/cod2-silicon/releases?per_page=10")!)
                request.timeoutInterval = 15; request.setValue("CoD2-Silicon-Launcher", forHTTPHeaderField: "User-Agent")
                request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
                let (data, response) = try await URLSession.shared.data(for: request)
                guard let response = response as? HTTPURLResponse, response.statusCode == 200 else { throw LauncherError(message: "Release service is unavailable. Try again later.") }
                let feed = try ReleaseNotes.parse(data)
                if !feed.dispatches.isEmpty { library.dispatches = feed.dispatches }
                library.dispatchesCheckedAt = Date(); saveLibrary()
                if let latest = feed.latestVersion, ReleaseNotes.isNewer(latest, than: version) {
                    updateMessage = "Version \(latest) is available. Download it when you're ready."
                    releaseURL = URL(string: "https://github.com/STiXzoOR/cod2-silicon/releases")
                } else { updateMessage = "You're up to date · \(version)"; releaseURL = nil }
            } catch { updateMessage = error.localizedDescription }
        }
    }

    // MARK: Preview data for the review harness (documentation-range addresses, invented names)
    private func seedPreview() {
        onboard = false; dataPath = "/Users/Player/Games/CoD2"; keyVerified = true; playerName = "^3Sgt. ^7Morrow"
        let rows: [(String, String, String, Int, Int, Int, Bool, String, Int, Bool, String)] = [
            ("^4Northern ^7Lights | Public", "mp_toujane", "tdm", 18, 24, 18, true, "", 250, false, "203.0.113.24:28960"),
            ("^2Sunday ^7Rifles · EU", "mp_carentan", "tdm", 15, 24, 30, false, "", 0, false, "198.51.100.7:28960"),
            ("^3Desert ^7Fox ^1SD", "mp_matmata", "sd", 12, 20, 42, true, "zPAM 3", 250, false, "203.0.113.90:28962"),
            ("^1Classic ^7Headquarters", "mp_dawnville", "hq", 9, 24, 54, false, "", 0, false, "198.51.100.41:28960"),
            ("^5Silicon ^7Sessions", "mp_leningrad", "tdm", 6, 16, 66, true, "", 250, true, "192.0.2.10:28960"),
            ("^7Eastern ^1Front", "mp_stalingrad", "dm", 20, 32, 71, false, "", 0, false, "198.51.100.88:28961"),
            ("^2Hedgerow ^7League", "mp_brecourt", "sd", 10, 10, 35, true, "zPAM 3", 250, false, "203.0.113.150:28960"),
            ("^6Railyard ^7Night", "mp_railyard", "ctf", 0, 20, 88, true, "", 250, false, "192.0.2.77:28960"),
        ]
        let names = ["Aurora", "^5North", "Moss", "Echo", "^3Scout", "River", "Harbor", "Quill", "Juniper", "Atlas", "Bramble", "Cedar"]
        servers = rows.map { row in
            var fields = ["sv_hostname": row.0, "mapname": row.1, "g_gametype": row.2, "clients": String(row.3), "sv_maxclients": String(row.4),
                          "shortversion": row.6 ? "1.4.6.8" : "1.3", "fs_game": row.7, "pswrd": row.9 ? "1" : "0"]
            if row.8 > 0 { fields["sv_maxfps"] = String(row.8) }
            return GameServer(address: row.10, fields: fields, ping: row.5,
                              players: (0..<min(row.3, 9)).map { ServerPlayer(id: $0, score: max(0, 42 - $0 * 5), ping: 18 + $0 * 7, name: names[$0]) })
        }
        selectedServer = servers.first?.address
        library.favorites = [servers[0].address, servers[2].address]
        library.recent = [servers[0].address, servers[1].address, servers[5].address]
        library.cachedAt = Date()
        settings.advanced = "com_hunkMegs 256"
        savedSettings = settings
        let calendar = Calendar(identifier: .gregorian)
        func date(_ day: Int, _ hour: Int, _ minute: Int) -> Date { calendar.date(from: DateComponents(year: 2026, month: 10, day: day, hour: hour, minute: minute)) ?? Date() }
        media = [("toujane_tdm_1004_2114.dm_1", date(4, 21, 14), 3_100_000), ("carentan_tdm_1003_2240.dm_1", date(3, 22, 40), 4_800_000),
                 ("leningrad_tdm_1003_1918.dm_1", date(3, 19, 18), 2_000_000)].map {
            MediaEntry(url: home.appendingPathComponent("main/demos/" + $0.0), facts: MediaFacts(name: $0.0, date: $0.1, bytes: $0.2))
        }
    }
    /// Screen states for the review harness.
    func preview(_ screen: String) {
        switch screen {
        case "setup-data": onboard = true; step = 0
        case "setup-data-missing": onboard = true; step = 0; dataPath = ""
        case "setup-key": onboard = true; step = 1; keyInput = "7Q4M 2KX9 LP3R"
        case "setup-shaders": onboard = true; step = 2; shaderBusy = true; shaderCount = 518; shaderMessage = "Extracting from Call of Duty 2 Multiplayer…"
        case "setup-ready": onboard = true; step = 2; shaderCount = 834; shaderMessage = "All shaders verified"
        case "servers": page = .servers
        case "servers-empty": page = .servers; search = "zzz"
        case "settings": page = .settings
        case "library-demos": page = .library
        case "library-screenshots": page = .library; libraryTab = .screenshots
            media = ["toujane-dusk.jpg", "carentan-rooftops.jpg", "leningrad-snow.jpg"].enumerated().map { index, name in
                MediaEntry(url: home.appendingPathComponent("main/screenshots/" + name),
                           facts: MediaFacts(name: name, date: Calendar(identifier: .gregorian).date(from: DateComponents(year: 2026, month: 10, day: 4 - min(index, 1), hour: 21 - index * 2, minute: 17 + index * 13)), bytes: 412_000))
            }
        case "about": page = .about
        case "home-first": library.recent = []
        // The review harness attaches a session over a temporary home with synthetic loadouts.
        case "configs-guided", "configs-guided-rules": page = .configs; studio.level = .guided; studio.group = .frame
        case "configs-expert-rules": page = .configs; studio.level = .expert; studio.group = .net
        case "configs-guided-aim", "configs-running": page = .configs; studio.level = .guided; studio.group = .aim
        case "configs-expert": page = .configs; studio.level = .expert; studio.group = .aim
        case "configs-text": page = .configs; studio.level = .expert; studio.group = .aim
        case "configs-history": page = .configs; studio.tab = .history
        // Discover and import: the harness supplies a signed fixture directory or a reviewed import.
        case "discover-unavailable", "discover-players": page = .configs; discover.page = .players
        case "discover-saved": page = .configs; discover.page = .saved
        case "discover-player": page = .configs
        case "configs-import", "configs-import-everything": page = .configs
        case "servers-ready", "servers-ready-fixed": page = .servers
        case "configs-binds-list", "configs-binds-visual", "configs-binds-advanced": page = .configs; studio.tab = .binds
        case "configs-checkup": page = .configs; studio.tab = .checkup
        default: page = .deploy
        }
    }
}

extension Animation {
    /// The launcher's one spring. Views read Reduce Motion and skip it where motion isn't meaning.
    static let launcherSpring = Animation.spring(response: 0.42, dampingFraction: 0.86)
}
