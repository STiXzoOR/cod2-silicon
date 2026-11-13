import AppKit
import SwiftUI

// The launcher's one model. Shared file: stored state sits in per-unit MARK regions below (add yours
// to your own region only); each unit's methods live in its own extension file (ModelSetup.swift,
// ModelServers.swift, ModelLibrary.swift, ModelSettings.swift, ConfigLauncherModel.swift). This file
// keeps the core: start-up, navigation, windows and sheets, the game's lifecycle and the harness states.

/// The four places in the sidebar (⌘1–⌘4). Settings is its own window, About one of its panes.
enum LauncherPage: String, CaseIterable, Identifiable, Hashable {
    case home, servers, library, configs
    var id: String { rawValue }
    var title: String {
        switch self { case .home: "Home"; case .servers: "Servers"; case .library: "Library"; case .configs: "Configs" }
    }
    var symbol: String {
        switch self {
        case .home: "house"; case .servers: "dot.radiowaves.left.and.right"; case .library: "film"; case .configs: "square.stack"
        }
    }
    /// The digit of its ⌘ shortcut.
    var shortcut: Character {
        switch self { case .home: "1"; case .servers: "2"; case .library: "3"; case .configs: "4" }
    }
    static let navigation: [LauncherPage] = [.home, .servers, .library, .configs]
}

/// The Settings window's panes, in toolbar order (SettingsWin board).
enum SettingsPane: String, CaseIterable, Identifiable, Hashable {
    case general, display, gameData, privacy, updates, advanced, about
    var id: String { rawValue }
    var title: String {
        switch self {
        case .general: "General"; case .display: "Display"; case .gameData: "Game Data"; case .privacy: "Privacy"
        case .updates: "Updates"; case .advanced: "Advanced"; case .about: "About"
        }
    }
    var symbol: String {
        switch self {
        case .general: "gearshape"; case .display: "display"; case .gameData: "folder"; case .privacy: "lock"
        case .updates: "arrow.down.to.line"; case .advanced: "slider.horizontal.3"; case .about: "info.circle"
        }
    }
}

/// The launcher's two windows, for sheets and alerts that must attach to the one in front.
enum LauncherWindowID: String { case main = "launcher", settings }

/// Where updates stand, for Home's pill, Settings › Updates and the app menu.
enum UpdateState: Equatable {
    case notChecked
    case checking
    case upToDate(version: String)
    /// A newer release this app can install (`updateOffer`), or only view (`releaseURL`). Notes: `releaseNotes`.
    case ready(version: String, installable: Bool)
    case installing(message: String)
    case failed(message: String)
}

enum ServerFilter: String, CaseIterable, Identifiable { case all = "All", stock = "Original 1.3", codx = "CoD2x 1.4.6.8"; var id: String { rawValue } }
enum ServerSort: String { case ping, players, name }
enum LibraryTab: String, CaseIterable, Identifiable { case matches = "Matches", demos = "Demos", screenshots = "Screenshots"; var id: String { rawValue } }

extension GameEngine {
    var dialect: ConfigDialect { self == .cod2x ? .cod2x : .stock }
    var catalogMode: ConfigCatalogMode { self == .cod2x ? .cod2x : .stock }
}

struct MediaEntry: Identifiable {
    var url: URL
    var facts: MediaFacts
    var id: String { url.path }
}

@MainActor final class LauncherModel: ObservableObject {
    // MARK: - Core state (ship-foundation)
    @Published var page: LauncherPage = .home
    @Published var onboard = true
    @Published var gameRunning = false
    /// The toast at the bottom of the main window (things that happened elsewhere).
    @Published var notice = ""
    @Published var crashReport: URL?
    /// The sheet over the main window and over the Settings window. Present with `present(_:)`.
    @Published var sheet: AppSheet?
    @Published var settingsSheet: AppSheet?
    /// A single decision over either window. Ask with `confirm(_:)`.
    @Published var confirmation: ConfirmationRequest?
    @Published var settingsConfirmation: ConfirmationRequest?
    /// The Settings window's selected pane; `openSettings(_:)` sets it and brings the window forward.
    @Published var settingsPane: SettingsPane = .general
    /// Opens a window scene by id ("launcher", "settings"). The app's scenes register it; the harness its own.
    var openWindowHandler: ((LauncherWindowID) -> Void)?
    weak var launcherWindow: NSWindow?
    weak var settingsWindow: NSWindow?
    @Published var playerName = PlayerProfile.unnamed
    @Published var keyVerified = false
    @Published var dataPath = ""
    @Published var approximate = false
    @Published var library = LauncherLibrary()
    @Published var settings = GameSettings() {
        didSet {
            if settingsReady && settings != oldValue { scheduleSettingsSave() }
            if settings.matchSummariesEnabled != oldValue.matchSummariesEnabled {
                studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
            }
            if settings.engine != oldValue.engine { followEngine() }
            if settingsReady && settings.resolution != oldValue.resolution { normalizeResolution() }
            if settingsReady && (settings.fullscreen != oldValue.fullscreen || settings.advanced != oldValue.advanced) { refreshDisplay() }
        }
    }
    let artwork: MapArtworkStore
    let home: URL
    let preferences: URL
    let snapshot: Bool
    let configStudio: ConfigStudioSession?
    let serverConnection = ConfigServerConnection()
    /// The Configs page's view store over the same session.
    let studio: ConfigStudioModel
    /// Discover and import. The player directory has no signed publication yet, so it reports itself unavailable.
    let discover: ConfigDiscoverModel
    /// The engines this app carries. Without the original one, Settings explains and CoD2x plays.
    var installedEngines: Set<GameEngine>
    var engine: Process?
    /// What each engine can't hold of the other's saved settings, from the catalogue.
    var engineLimits: ConfigEngineLimits?
    var attachedGames: [pid_t: NSRunningApplication] = [:]
    var attachmentTask: Task<Void, Never>?
    var connectionTask: Task<Void, Never>?
    var handOffTask: Task<Void, Never>?
    var gameStartedAt = Date()
    var lastExitCode: Int32?
    var reportTask: Task<Void, Never>?
    @Published var pendingLink: LaunchLink?
    @Published var fastPlay = false
    var booted = false
    var forwardedArguments: [String] = []
    var exitAfterGame = false
    /// Creates the native export dialog; callers configure it before presentation.
    var makeExportPanel: @MainActor () -> NSSavePanel = { NSSavePanel() }

    // MARK: - ship-shell state (setup, sidebar, player card, Jump)
    @Published var sidebarVisibility = NavigationSplitViewVisibility.all
    static let shaderTotal = 834
    @Published var step = 0
    @Published var dataError = ""
    @Published var attemptedDataPath = ""
    @Published var setupError = ""
    @Published var replacingKey = false
    @Published var setupReturnPage: LauncherPage?
    var confirmedDataPath = ""
    var dataChanged = false
    var makeSetupPanel: @MainActor () -> NSOpenPanel = { NSOpenPanel() }
    @Published var keyInput = ""
    @Published var shaderBusy = false
    @Published var shaderCount = 0
    @Published var shaderMessage = "Lighting, sky and water as the game draws them on Windows, built on this Mac from your own game files."
    var setupTask: Task<Void, Never>?
    var shaderRefresh: Task<Void, Never>?
    /// Jump to… (⌘K) is open over the main window.
    @Published var jumpPresented = false
    /// The query in the launcher-wide Jump to… palette.
    @Published var jumpQuery = ""
    /// The player card popover over the sidebar's plate.
    @Published var playerCardPresented = false
    /// Player card: the last name save, said under the card (“Saved to your Main loadout.”).
    @Published var playerCardStatus: String?
    @Published var playerCardError: String?
    var playerNameSave: Task<Void, Never>?
    /// Setup › Make it yours: what is typed and chosen there, applied only on Continue.
    @Published var setupName = ""
    @Published var setupDisplayMode = ""
    @Published var setupNameError: String?
    /// Setup › Game data: how many of the 16 archives the refused folder holds.
    @Published var dataArchivesFound = 0
    /// Setup › Shaders: the build failed (approximate shaders), and how long a finished build took.
    @Published var shaderFailed = false
    @Published var shaderSeconds: Int?
    /// A join link that arrived during setup: Connect to Address opens with it when setup ends.
    var pendingJoinAddress: String?
    /// The player chose Quit Launcher Only or Quit Both, so quitting no longer asks.
    var quitConfirmed = false
    var quitGameTask: Task<Void, Never>?
    /// The game was already running when the launcher started: its window hides when it first appears.
    var hideLauncherWindowOnAppear = false
    /// Why the last Launch failed, for the Game Couldn't Start sheet's fix.
    @Published var launchProblem: LaunchProblem?

    // MARK: - ship-settings state (settings saving, CoD2x files, updates)
    @Published var settingsSaveError: String?
    @Published var advancedDraft: String?
    @Published var advancedError: String?
    var settingsReady = false
    var settingsSaveTask: Task<Void, Never>?
    var settingsTerminationObserver: NSObjectProtocol?
    var savedSettings = GameSettings()
    /// Settings › Game Data › CoD2x menus and icons, kept in the app home (CoD2xFiles).
    @Published var cod2xFilesInstalled = false
    @Published var cod2xFilesBusy = false
    @Published var cod2xFilesMessage = CoD2xFiles.Message.notInstalled
    var cod2xFilesTask: Task<Void, Never>?
    @Published var updateMessage = "Check for the latest release when you're ready."
    @Published var checkingUpdates = false
    @Published var updateWasChecked = false
    @Published var updateError = false
    @Published var releaseURL: URL?
    /// The newer release Install and Relaunch installs, when it carries this Mac's archive and SHA256SUMS.
    @Published var updateOffer: AppUpdate.Offer?
    @Published var installingUpdate = false
    /// The check or install in flight (Cancel stops a download), and the daily automatic check.
    var updateTask: Task<Void, Never>?
    var automaticUpdateTask: Task<Void, Never>?
    /// What failed last, the check or the install, so Updates says which and Try Again repeats it.
    @Published var updateFailedStage: UpdateStage?
    /// The newer release's notes for What's New, from its release text.
    @Published var updateNotes: [ReleaseNoteItem] = []
    /// The version a check found, also when this Mac can't install it (then only the release page opens).
    @Published var availableVersion: String?
    /// Settings › Game Data: what was found (read when the pane opens), why a chosen game folder wasn't
    /// used, and what Rebuild did.
    @Published var gameData = GameDataFacts()
    @Published var dataChangeError: String?
    /// A short confirmation in the pane where the person acted.
    @Published var settingsFeedback: SettingsFeedback?
    /// Acknowledgements: the component whose licence is showing.
    @Published var selectedAcknowledgement: String?
    /// Integration hook: called with (version, installsOnQuit) when the automatic check finds a newer release.
    /// Connected to shell notifications by `startSettings()`.
    var onUpdateReady: ((String, Bool) -> Void)?
    @Published var shaderRebuildMessage: String?
    /// Settings › Privacy › See What's Sent… is open.
    @Published var privacyDetailsShown = false
    /// Settings › Display: the Mac's main display, read when the pane opens and when displays change.
    @Published var mainDisplay: MainDisplay?
    var displayObserver: NSObjectProtocol?
    /// The appearance last given to the app, so saves that don't change it leave the windows alone.
    var appliedAppearance: LauncherAppearance?

    // MARK: - ship-servers state (list, filters, connection)
    @Published var servers: [GameServer] = []
    @Published var selectedServer: String? {
        didSet { do { try serverConnection.select(address: selectedServer) } catch { serverConnection.clear(); notice = error.localizedDescription } }
    }
    @Published var refreshing = false
    @Published var search = ""
    @Published var sort: ServerSort = .ping
    @Published var sortAscending = true
    @Published var serverSearchRequested = false
    /// All · Favorites · Recent, the switch at the left of the Servers toolbar.
    @Published var serverScope: ServerScope = .all
    /// The Filters popover's choices; saved in launcher-servers.json whenever they change.
    @Published var serverFilters = ServerFilters() { didSet { if serverFilters != oldValue { saveServerPrefs() } } }
    @Published var serverFiltersPresented = false
    /// While refreshing: servers asked, finished (answered or timed out) and answered.
    @Published var refreshAsked = 0
    @Published var refreshFinished = 0
    @Published var refreshAnswered = 0
    /// The last refresh got no list from either master server; the cached list is what's shown.
    @Published var mastersDidNotAnswer = false
    /// Servers › Launch on a server that runs the other game version: the engine for that one match.
    @Published var matchEngine: (address: String, engine: GameEngine)?
    /// The AnyCancellable (ModelServers.swift) that ends a one-match switch when the game closes.
    var matchEngineReset: AnyObject?
    var serverPrefs = ServerBrowserPrefs()
    var serverPrefsLoaded = false
    /// The review harness keeps remembered passwords in memory instead of the Keychain.
    var previewPasswords: [String: String] = [:]
    var browserTask: Task<Void, Never>?
    /// Servers asked for their version before Original 1.3 joins them: answered (or not), and in flight.
    var checkedAddresses: Set<String> = []
    var lookingUp: Set<String> = []
    lazy var readyRules = try? ConfigRules.load()
    var readyChecker: ConfigReadyCheck? {
        guard let catalog = studio.catalog, let rules = readyRules else { return nil }
        return ConfigReadyCheck(catalog: catalog, rules: rules)
    }

    // MARK: - ship-home-library state (Home, demos and screenshots)
    @Published var media: [MediaEntry] = []
    @Published var libraryTab: LibraryTab = .demos
    @Published var selectedMedia: URL?
    @Published var mediaMessage: String?
    @Published var mediaError = false
    @Published var renamingMedia: MediaEntry?
    /// The file Move to Trash last moved (its place before and in the Trash), for the banner's Undo.
    @Published var trashedMedia: (original: URL, trashed: URL)?
    /// Each Library tab's scroll offset, kept while the launcher runs.
    var libraryOffsets: [LibraryTab: CGFloat] = [:]
    /// Game data that was found once and has since gone missing (Home's attention chip).
    @Published var dataMissing = false
    /// Quit Game was clicked and the game hasn't closed yet.
    @Published var quittingGame = false

    // MARK: - ship-configs state (Configs lives in `studio` and `discover`)

    // MARK: - ship-play
    @Published var resolutionAdjustment: String?
    var sessionLog: GameSessionLog?
    var displaySubscription: Any?

    // MARK: - Core
    var gameBundle: URL { Bundle.main.bundleURL.appendingPathComponent("Contents/Helpers/CoD2 Game.app") }
    /// Either engine of this app, as running processes report their executables.
    var gameExecutables: Set<URL> { Set(GameEngine.allCases.map { $0.executable(in: gameBundle).resolvingSymlinksInPath() }) }
    /// The engine the next game starts with: the one chosen in Settings, when this app carries it.
    var chosenEngine: GameEngine { matchEngineOverride ?? (installedEngines.contains(settings.engine) ? settings.engine : .cod2x) }
    var originalAvailable: Bool { installedEngines.contains(.original) }
    var activeGamePID: pid_t? {
        if let engine, engine.isRunning { return engine.processIdentifier }
        if let app = attachedGames.values.filter({ !$0.isTerminated }).sorted(by: { $0.processIdentifier < $1.processIdentifier }).first { return app.processIdentifier }
        return nil
    }

    init(snapshot: Bool = false, catalog: @escaping @Sendable () throws -> ConfigCatalog = { try ConfigStudioModel.bundledCatalog() }) {
        self.snapshot = snapshot
        home = URL(fileURLWithPath: snapshot ? "/Preview/CoD2 Silicon" : LauncherHome())
        artwork = MapArtworkStore(home: home)
        preferences = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent(".cod2/preferences")
        let gameApp = Bundle.main.bundleURL.appendingPathComponent("Contents/Helpers/CoD2 Game.app")
        installedEngines = snapshot ? Set(GameEngine.allCases) : GameEngine.installed(in: gameApp)
        let expected = Set(GameEngine.allCases.map { $0.executable(in: gameApp).resolvingSymlinksInPath() })
        let identifier = (Bundle.main.bundleIdentifier ?? "") + ".game"
        var sessionError: String?
        if snapshot { configStudio = nil }
        else {
            do {
                configStudio = try ConfigStudioSession(home: home, dialect: .cod2x, discoverGames: {
                    Set(NSRunningApplication.runningApplications(withBundleIdentifier: identifier).filter {
                        !$0.isTerminated && $0.executableURL.map { expected.contains($0.resolvingSymlinksInPath()) } == true
                    }.map(\.processIdentifier))
                })
            } catch { configStudio = nil; sessionError = error.localizedDescription }
        }
        studio = ConfigStudioModel(session: configStudio, dialect: .cod2x, catalog: catalog)
        discover = ConfigDiscoverModel(studio: studio, directory: ConfigDirectory(cacheURL: home.appendingPathComponent("directory/index.json")))
        discover.notify = { [weak self] in self?.notice = $0 }
        if let sessionError { notice = sessionError }
        if snapshot { seedPreview(); return }
        mainDisplay = MainDisplay.current()
        if let display = mainDisplay { settings.resolution = DisplayResolutions.initial(native: display.native) }
        if let mode = Bundle.main.object(forInfoDictionaryKey: "CoD2DefaultFullscreen") as? String { settings.fullscreen = mode }
        LauncherMigrate()
        if let data = try? Data(contentsOf: home.appendingPathComponent("launcher-settings.json")), let value = try? JSONDecoder().decode(GameSettings.self, from: data) { settings = value }
        else if let value = ConfigDocument.launchLoadout(home: home, profileID: nil)?.effectiveSetting(named: "r_mode"), DisplayResolution(value) != nil {
            settings.resolution = value // An existing player's game choice predates launcher settings.
        }
        savedSettings = settings
        // Old extra options could override the picker. Move that choice into the one visible control.
        let advancedResolution = settings.advanced.components(separatedBy: "\n").compactMap(GameSettings.advancedPair).last { $0.0.lowercased() == "r_mode" }?.1
        if let value = advancedResolution, DisplayResolution(value) != nil { settings.resolution = value }
        settings.advanced = settings.advanced.components(separatedBy: "\n").filter { GameSettings.advancedPair($0)?.0.lowercased() != "r_mode" }.joined(separator: "\n")
        refreshDisplay()
        settingsReady = true
        if settings != savedSettings { scheduleSettingsSave() }
        settingsTerminationObserver = NotificationCenter.default.addObserver(forName: NSApplication.willTerminateNotification, object: nil, queue: .main) { [weak self] _ in
            MainActor.assumeIsolated { self?.flushSettings() }
        }
        displaySubscription = studio.objectWillChange.sink { [weak self] _ in
            Task { @MainActor [weak self] in self?.refreshDisplay() }
        }
        followEngine()
        studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
        if let data = try? Data(contentsOf: home.appendingPathComponent("launcher-library.json")), let value = try? JSONDecoder().decode(LauncherLibrary.self, from: data) { library = value; servers = value.cache }
        dataPath = LauncherFindData(nil) ?? ""
        refreshCoD2xFiles()
        keyVerified = KeyStore.hasValidKey(at: preferences)
        approximate = !LauncherVerifyShaders()
        shaderCount = approximate ? 0 : Self.shaderTotal
        confirmedDataPath = dataPath
        readPlayerName()
        let setupComplete = FileManager.default.fileExists(atPath: home.appendingPathComponent(".launcher-setup-complete").path)
        // ship-shell: a finished setup isn't asked again for a skipped CD key or approximate shaders.
        onboard = dataPath.isEmpty || (!setupComplete && (!keyVerified || approximate))
        // Show the detected folder before asking for a key, so it can be reviewed or changed.
        step = 0
    }
    var version: String { Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.3.0" }
    /// This build's crash report receiver (a Sentry DSN written at build time); without one nothing is ever sent
    /// and Settings doesn't offer it. The review harness shows the switch with a placeholder that is never used.
    var crashReportService: SentryDSN? {
        snapshot ? SentryDSN("https://preview@sentry.invalid/1") : SentryDSN(Bundle.main.object(forInfoDictionaryKey: CrashReporting.infoKey) as? String ?? "")
    }
    var dataPathDisplay: String { (dataPath as NSString).abbreviatingWithTildeInPath }
    func boot() {
        guard !snapshot else { return }
        booted = true
        startSettings()  // ship-settings: appearance, the last Settings pane, the daily update check
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
        if ProcessInfo.processInfo.environment["COD2_SETUP_NONINTERACTIVE"] != "1" { refreshShaders() }
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
    func readPlayerName() {
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

    // MARK: Navigation
    var canNavigate: Bool { !onboard && !gameRunning && !installingUpdate }
    var canImport: Bool { canNavigate && discover.step == nil }
    func navigate(to destination: LauncherPage) {
        guard canNavigate else { return }
        page = destination
        if destination == .library { loadMedia() }
        if destination == .configs { studio.loadCatalog() }
    }

    // MARK: Windows, sheets and confirmations
    /// Brings the main window forward (reopening it if it was closed).
    func showMainWindow() {
        openWindowHandler?(.main)
        NSApp.activate(ignoringOtherApps: true)
    }
    /// The one way into Settings: selects `pane` (or keeps the last one) and brings the window forward.
    func openSettings(_ pane: SettingsPane? = nil) {
        if let pane { settingsPane = pane }
        openWindowHandler?(.settings)
        settingsWindow?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
    /// Whether the Settings window is the one in front, so a sheet or an alert attaches to it.
    var settingsInFront: Bool { settingsWindow.map { $0.isVisible && $0.isKeyWindow } ?? false }
    /// Shows `sheet` attached to the window in front (Settings when it's key, otherwise the main window).
    func present(_ sheet: AppSheet) {
        if settingsInFront { settingsSheet = sheet; return }
        if launcherWindow?.isVisible != true { openWindowHandler?(.main) }
        self.sheet = sheet
    }
    func dismissSheet() { sheet = nil; settingsSheet = nil }
    /// Asks a single question as an alert on the window in front.
    func confirm(_ request: ConfirmationRequest) {
        if settingsInFront { settingsConfirmation = request } else { confirmation = request }
    }

    // MARK: Menu commands every unit can call (Menus board; ship-shell builds the menus)
    /// File › Open Game Menu (⇧⌘↩): the game's own menu, without joining a server.
    // MARK: - ship-integrate
    func openGameMenu() {
        pendingLink = nil; fastPlay = false
        try? serverConnection.select(address: nil)
        matchEngine = nil; followEngine()
        play()
    }
    /// File › Show Game Data in Finder.
    func showGameDataInFinder() {
        guard !dataPath.isEmpty else { return }
        NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: dataPath)])
    }
    /// File › Show My Files in Finder: the app home (configs, demos, screenshots, reports).
    func showMyFilesInFinder() { openHomeFolder() }
    /// Help › Show Logs in Finder: latest session output, then an older crash/freeze report or the app folder.
    func showLogsInFinder() {
        if let report = GameSessionLog.latest(home: home) ?? CrashReports.latest(in: home, since: .distantPast) { NSWorkspace.shared.activateFileViewerSelecting([report]) }
        else { openHomeFolder() }
    }
    /// Help › CoD2 Silicon Help: the project's documentation page.
    func openHelp() { NSWorkspace.shared.open(URL(string: "https://github.com/STiXzoOR/cod2-silicon")!) }
    /// Help › Release Notes.
    func openReleaseNotes() { NSWorkspace.shared.open(AppUpdate.releases) }
    /// Help › Keyboard Shortcuts.
    func showKeyboardShortcuts() { present(.keyboardShortcuts) }
    /// Help › Report a Problem… and Settings › About.
    func reportProblem() { present(.reportProblem) }
    /// View › Hide Sidebar / Show Sidebar (⌃⌘S).
    func toggleSidebar() { sidebarVisibility = sidebarVisibility == .detailOnly ? .all : .detailOnly }
    /// Edit › Find (⌘F): focuses the current page's search. Servers listens to `serverSearchRequested`;
    /// Configs routes to the search in its active section.
    func findInPage() {
        switch page {
        case .servers: serverSearchRequested.toggle()
        case .home, .library: break
        case .configs: findInConfigs()  // ship-configs
        }
    }
    /// View › Jump to… (⌘K): Configs searches settings; elsewhere it searches the launcher.
    func showJump() {
        guard canNavigate, !jumpToSetting() else { return }
        jumpQuery = ""; jumpPresented = true
    }

    // MARK: Loadouts and engine
    var usesLoadouts: Bool { (studio.session ?? configStudio)?.migrationCompleted == true }
    /// Config Studio reads, checks and edits loadouts in the language of the engine the next game starts with.
    func followEngine() {
        let dialect = chosenEngine.dialect
        do {
            try configStudio?.setDialect(dialect)
            if let attached = studio.session, attached !== configStudio { try attached.setDialect(dialect) }
        } catch { notice = error.localizedDescription }
        studio.setDialect(dialect)
    }
    func saveLibrary() {
        guard !snapshot else { return }
        do { try PrivateFile.write(JSONEncoder().encode(library), to: home.appendingPathComponent("launcher-library.json")) }
        catch { notice = "Could not save your server library." }
    }

    // MARK: Launching
    /// Home's Launch: rejoin the last server, or open the game's menu when there is none.
    func launch() {
        guard !onboard else { return }
        if let address = library.recent.first { join(address) }
        else { openGameMenu() }
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
        sessionLog?.protect(password: link.password)
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
        let expected = gameExecutables
        let apps = NSRunningApplication.runningApplications(withBundleIdentifier: identifier + ".game").filter {
            !$0.isTerminated && $0.executableURL.map { expected.contains($0.resolvingSymlinksInPath()) } == true
        }
        guard let app = apps.first else { return false }
        attachedGames = Dictionary(uniqueKeysWithValues: apps.map { ($0.processIdentifier, $0) })
        gameRunning = true; gameStartedAt = Date()
        do { try configStudio?.reconcileGames(Set(attachedGames.keys)) } catch { notice = error.localizedDescription }
        // ship-shell: hide the windows if the player wants them hidden while playing (ModelShell.swift).
        gameTookOver()
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
        let expected = gameExecutables
        let identifier = (Bundle.main.bundleIdentifier ?? "") + ".game"
        let apps = NSRunningApplication.runningApplications(withBundleIdentifier: identifier).filter {
            !$0.isTerminated && $0.executableURL.map { expected.contains($0.resolvingSymlinksInPath()) } == true
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
        if let demo {
            guard !gameRunning else { mediaError = true; mediaMessage = "Close the game before playing a recording."; return }
            guard mediaFileExists(demo) else { return }
            pendingLink = nil
        }
        guard !snapshot, !gameRunning else { return }
        refreshDisplay()
        flushSettings()
        if let refresh = shaderRefresh { Task { await refresh.value; play(demo: demo) }; return }
        if attachExistingGame() {
            if demo != nil { mediaError = true; mediaMessage = "Close the game before playing a recording." }
            if let link = pendingLink { pendingLink = nil; openLink(link.url) }
            return
        }
        if onboard { return }
        if demo == nil, let link = pendingLink, !admit(link) { return }
        do {
            try rememberData()
            let gameEngine = chosenEngine
            let executable = gameEngine.executable(in: gameBundle)
            guard FileManager.default.isExecutableFile(atPath: executable.path) else { throw LauncherError(message: "The game helper is missing. Rebuild or reinstall this app.") }
            // CoD2x's menus and icons for a CoD2x game, none for the original 1.3. Missing files never stop a game.
            _ = CoD2xFiles.prepare(for: gameEngine, home: home, dataPath: dataPath)
            // Values the other engine couldn't keep go back before this one reads the loadout.
            if let configStudio {
                studio.loadCatalogNow()
                if engineLimits == nil, let catalog = studio.catalog { engineLimits = ConfigEngineLimits(catalog: catalog) }
                if let engineLimits { try configStudio.handOff(to: gameEngine.dialect, limits: engineLimits) }
            }
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
            let loadout = ConfigDocument.launchLoadout(home: home, profileID: configLaunch?.profileID)
            let plan = try GameLaunchPlan(settings: settings, dataPath: dataPath, homePath: home.path, link: pendingLink,
                demo: demo?.deletingPathExtension().lastPathComponent, forwarded: forwardedArguments, configStudio: configLaunch, connection: connectionLaunch,
                loadout: { loadout?.effectiveSetting(named: $0) })
            let process = Process(); process.executableURL = executable; process.arguments = plan.arguments
            process.currentDirectoryURL = home
            var environment = ProcessInfo.processInfo.environment
            environment["HOME"] = NSHomeDirectory()
            environment.removeValue(forKey: "COD2_SETUP_CD_KEY")
            environment["SDL_VIDEO_MAC_FULLSCREEN_SPACES"] = settings.fullscreen == "spaces" ? "1" : "0"
            if !approximate { environment["COD2_MAC_SHADER_CACHE"] = home.appendingPathComponent("shaders").path }
            else { environment.removeValue(forKey: "COD2_MAC_SHADER_CACHE") }
            // CoD2x's own reports go to CoD2x only with the player's agreement (Settings › Privacy), and only from CoD2x.
            CrashReporting.applyConsent(to: &environment, engine: gameEngine, cod2xReports: settings.sendCoD2xReports)
            process.environment = environment
            process.standardInput = FileHandle.standardInput
            let log = try GameSessionLog(home: home, password: plan.link?.password)
            sessionLog = log
            process.standardOutput = log.pipe; process.standardError = log.pipe
            process.terminationHandler = { [weak self] process in
                log.finish()
                let code = process.terminationStatus
                let pid = process.processIdentifier
                Task { @MainActor in
                    guard let self, self.engine?.processIdentifier == pid else { return }
                    self.lastExitCode = code; self.engine = nil; self.synchronizeGames()
                }
            }
            try PrivateFile.write(Data(try settings.launchConfig(migrated: configLaunch != nil).utf8), to: home.appendingPathComponent("main/launcher.cfg"))
            try process.run(); log.closeParentWriter(); engine = process; gameRunning = true; gameStartedAt = Date(); crashReport = nil; notice = ""
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
            sessionLog?.finish(); sessionLog = nil
            // ship-shell: the Game Couldn't Start sheet says why and offers the fix (ModelShell.swift).
            do { try configStudio?.cancelLaunch() } catch { notice = error.localizedDescription }
            launchFailed(error)
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
            // ship-shell: the launcher keeps its Dock icon; its windows hide if the player wants that.
            self.gameTookOver()
        }
    }
    /// Takes the Dock back after the game. The system can refuse the switch (seen right after a
    /// LaunchServices launch); AppKit then already reports .regular, so step through accessory and retry.
    private func restoreRegularPolicy() {
        // ship-shell: the launcher stays regular while the game runs, so there is nothing to take back.
        guard NSApp.activationPolicy() != .regular else { return }
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
        // 1 is Exit in CoD2x's machine identity dialog, a normal close.
        let closed = code == 0 || code == 1
        if code != nil && !closed {
            notice = "The game closed unexpectedly. Your settings and loadouts are safe."
        }
        if !closed {
            crashReport = GameSessionLog.latest(home: home) ?? CrashReports.latest(in: home, since: gameStartedAt)
        }
        if code == nil && crashReport != nil { notice = "The game closed unexpectedly. Your settings and loadouts are safe." }
        if !closed { sendCrashReports(CrashReports.all(in: home, since: gameStartedAt)) }
        print("CoD2 Silicon: returned to launcher (game exit \(code.map(String.init) ?? "unavailable")).")
        fflush(stdout)
        readPlayerName()
        refreshMatchSummaries()
        loadMedia()
        if pendingLink != nil { play() }
        else if exitAfterGame { NSApp.terminate(nil) }
    }

    /// New crash and freeze reports, classified now (before another game can start and send CoD2x's own),
    /// then sent in order. The crash notice says what happened to them.
    private func sendCrashReports(_ logs: [URL]) {
        guard !logs.isEmpty, !snapshot else { return }
        let sendOurs = settings.sendCrashReports, cod2x = settings.sendCoD2xReports, dsn = crashReportService, home = home
        let facts = CrashReporting.Facts.current(release: version)
        let owners = logs.map(CrashReporting.owner)
        let base = notice, previous = reportTask
        let pending = sendOurs && dsn != nil && owners.contains(.ours)
        if pending, !base.isEmpty { notice = base + " " + CrashReporting.sending }
        let shown = notice
        reportTask = Task { [weak self] in
            await previous?.value
            let status = await CrashReporting.report(logs: logs, owners: owners, home: home, sendOurs: sendOurs, cod2xReports: cod2x, dsn: dsn, facts: facts)
            guard let self, !base.isEmpty, self.notice == shown else { return }
            self.notice = status.map { base + " " + $0.message } ?? base
        }
    }

    /// Check-up reads the game's local match summaries from this home at startup and after each
    /// game; the 250 ms game monitor never reads them. The switch shown applies on the next launch.
    private func refreshMatchSummaries() {
        studio.checkup.matchSummariesEnabled = settings.matchSummariesEnabled
        studio.checkup.refreshMatches(home: home)
    }

    // MARK: Folders
    func openHomeFolder() { NSWorkspace.shared.open(home) }

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
    /// Screen states for the review harness: each unit's `preview<Unit>(_:)` sets up its own screens
    /// (in its model file); the core keeps the shared examples.
    func preview(_ screen: String) {
        if previewCore(screen) || previewShell(screen) || previewHomeLibrary(screen) || previewServers(screen) || previewSettings(screen) || previewConfigs(screen) { return }
        page = .home
    }
    /// Shared examples: the Foundations gallery and a sheet over each window.
    private func previewCore(_ screen: String) -> Bool {
        switch screen {
        case "foundations", "foundations-popover": page = .home
        case "sheet-keyboard-shortcuts": page = .home; sheet = .keyboardShortcuts
        case "settings-sheet-acknowledgements": settingsPane = .about; settingsSheet = .acknowledgements
        default: return false
        }
        return true
    }
}

extension Animation {
    /// The launcher's one spring. Views read Reduce Motion and skip it where motion isn't meaning.
    static let launcherSpring = Animation.spring(response: 0.42, dampingFraction: 0.86)
}

extension ConfigDocument {
    /// The profile config the game reads at launch: the launching loadout, or else the game's
    /// active profile (main/players/active.txt). nil when there is none yet, as on a first run.
    static func launchLoadout(home: URL, profileID: String?) -> ConfigDocument? {
        let players = home.appendingPathComponent("main/players")
        let active = (try? String(contentsOf: players.appendingPathComponent("active.txt"), encoding: .utf8))?
            .trimmingCharacters(in: .whitespacesAndNewlines)
        // MARK: - ship-play: an older install can have chosen its display before named loadouts existed.
        let path: URL
        if let id = profileID ?? active {
            guard (try? ConfigStudioLaunch.validateProfileID(id)) != nil else { return nil }
            path = players.appendingPathComponent(id).appendingPathComponent("config_mp.cfg")
        } else { path = home.appendingPathComponent("main/config_mp.cfg") }
        guard let data = try? Data(contentsOf: path) else { return nil }
        return ConfigDocument(data: data, dialect: .cod2x)
    }
}
